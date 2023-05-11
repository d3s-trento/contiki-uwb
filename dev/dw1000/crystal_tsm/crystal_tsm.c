#include PROJECT_CONF_H

#include "contiki.h"

#include "crystal_tsm.h"

#include "trex-driver.h" // required for TREXD_FRAME_OVERHEAD
#include "trex-tsm.h"
#include "trex.h"

#include "glossy_tsm.h"

#include "dw1000-conv.h"
#include "dw1000-util.h" // required for dw1000_estimate_tx_time
#include "dw1000-config.h" // dw1000_get_current_cfg

#define LOG_PREFIX "crystal_tsm"
#define LOG_LEVEL LOG_WARN
#include "logging.h"
#include "deployment.h"

#include "crystal_tsm_conf.h"
#include "crystal_bitmap_mapping.h"

#include "print-def.h"

#include PROJECT_CONF_H

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

#if TARGET == evb1000
#define CRYSTAL_PKTBUF_LEN 127 // Use max for dw1000
#endif

#pragma message STRDEF(CRYSTAL_SYNC_ACKS)
#pragma message STRDEF(CRYSTAL_VARIANT)
#pragma message STRDEF(START_DELAY_NONSINK)
#pragma message STRDEF(START_DELAY_SINK)

static struct pt crystal_pt;
static struct pt crystal_scan_pt;

extern uint32_t glossy_tsm_minislot_last_rx;

static crystal_context_t crystal_context;
static crystal_config_t crystal_conf = {

    .period  = CRYSTAL_CONF_PERIOD,
    .is_sink = CRYSTAL_CONF_IS_SINK,

    .ntx_S   = CRYSTAL_CONF_NTX_S,
    .plds_S  = 0,

    .ntx_T   = CRYSTAL_CONF_NTX_T,
    .plds_T  = 0,

    .ntx_A   = CRYSTAL_CONF_NTX_A,
    .plds_A  = 0,

    .r       = CRYSTAL_CONF_SINK_MAX_EMPTY_TS,
    .y       = CRYSTAL_CONF_MAX_SILENT_TAS,
    .z       = CRYSTAL_CONF_MAX_MISSING_ACKS,
    // .x       = CRYSTAL_CONF_SINK_MAX_NOISY_TS,
    .x       = CRYSTAL_CONF_SINK_MAX_RCP_ERRORS_TS,
    //.xa      = CRYSTAL_CONF_MAX_NOISY_AS,
    .xa      = CRYSTAL_CONF_MAX_RCP_ERRORS_AS,

    .scan_duration = CRYSTAL_MAX_SCAN_EPOCHS,
};

static union {
  uint8_t raw[CRYSTAL_PKTBUF_LEN];

  struct __attribute__((packed, aligned(1))) {
    uint8_t tsm_hdr[TSM_HDR_LEN];
    uint8_t type;
    union __attribute__((packed, aligned(1))) {
        crystal_sync_hdr_t sync_hdr;
        crystal_data_hdr_t data_hdr;
        crystal_ack_hdr_t ack_hdr;
    };
  };
} buf;     // buffer for TX and RX

#define CRYSTAL_S_HDR_LEN (sizeof(buf.type) + sizeof(crystal_sync_hdr_t))
#define CRYSTAL_T_HDR_LEN (sizeof(buf.type) + sizeof(crystal_data_hdr_t))
#define CRYSTAL_A_HDR_LEN (sizeof(buf.type) + sizeof(crystal_ack_hdr_t ))

static uint32_t crystal_max_tas_dyn;

#define CRYSTAL_MAX_TAS crystal_max_tas_dyn

crystal_info_t crystal_info;
crystal_app_log_t crystal_app_log;

#define BZERO_BUF() bzero(buf.raw, CRYSTAL_PKTBUF_LEN)

#define CRYSTAL_SCAN_TERMINATION (SINK_RADIUS + crystal_conf.ntx_S)

#if CRYSTAL_USE_DYNAMIC_NEMPTY
#define CRYSTAL_SINK_MAX_EMPTY_TS_DYNAMIC(n_ta_) (((n_ta_)>1)?(crystal_conf.r):1)
#warning ------------- !!! USING DYNAMIC N_EMPTY !!! -------------
#else
#define CRYSTAL_SINK_MAX_EMPTY_TS_DYNAMIC(n_ta_) crystal_conf.r
#endif

#if (CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS) && (CRYSTAL_VARIANT != CRYSTAL_VARIANT_SIMPLE)
#error "Crystal variant not recognized"
#endif

crystal_config_t crystal_get_config() {
    return crystal_conf;
}

int16_t get_n_ta(uint16_t logic_slot_idx) {
#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
  // First check if we are before the first TA (i.e. sink) and if we are we set n_ta to -1 (to signal we are not currently in any TA)
  if (logic_slot_idx < SINK_RADIUS + crystal_conf.ntx_S) {
    // NOTE:  We cannot have this being during the Flick slot as we cannot receive a valid packet during it
    return -1; // We are in the bootstrap/S phase
  }

  /* To get the n_ta at which we are from the slot index we first remove the initial sync
   * which lasts SINK_RADIUS + crystal_conf.ntx_S, and then divide for how long a TA lasts (2 SINK_RADIUS + ntx_T + ntx_A)
   */
  return (logic_slot_idx - SINK_RADIUS - crystal_conf.ntx_S)/(SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A);
#else
  // Note that regardless of the exact schema used for flick all variants organize the slots in the same way
  if (logic_slot_idx < SINK_RADIUS + crystal_conf.ntx_S + FS_MACROSLOT) {
    // NOTE:  We cannot have this being during the Flick slot as we cannot receive a valid packet during it
    return -1; // We are in the bootstrap/S phase
  }

  /* To get the n_ta at which we are from the slot index we first remove the initial sync
   * which lasts SINK_RADIUS + crystal_conf.ntx_S, and then divide for how long a TA lasts (2 SINK_RADIUS + ntx_T + ntx_A)
   */
  return (logic_slot_idx - SINK_RADIUS - crystal_conf.ntx_S - FS_MACROSLOT)/(SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A + FS_MACROSLOT);
#endif
}

uint32_t from_minislots_to_logic_slots(uint32_t minislots) {
#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
  /*
   * If we don't use Flick then all the logic slots are homogeneous (they all use the same number of minislots) 
   * so we can just get the number of minislots and divide by the minislots needed 
   */
  return minislots / TSM_DEFAULT_MINISLOTS_GROUPING;
#else
  /*
   * indexes
   *      minislots | 0| 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|
   *     logic slot |    0   |    1   |    2   |    3   |    4   |    5   |                   8                  |
   *
   * hops        0  |    T   |    T   |        |        |        |        |                Flick                 |
   *             1  |        |    T   |    T   |        |        |        |                Flick                 |
   *             2  |        |        |    T   |    T   |        |        |                Flick                 |
   *             3  |        |        |        |    T   |    T   |        |                Flick                 |
   *             4  |        |        |        |        |    T   |    T   |                Flick                 |
   *
   * For this example consider SINK_RADIUS = 4, ntx_S = 2
   * 1. Consider that a crystal bootstrap will last SINK_RADIUS + ntx_S logic slots
   * 2. As these logic slots are homogenous in size (TSM_DEFAULT_MINISLOTS_GROUPING), 
   *    the first minislot of the last bootstrap logic slot will be TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + ntx_S)
   * 3. We can thus, when considering the first w < TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + ntx_S) slots, just divide
   *    to obtain the current logic slot index
   * 4. We can calculate the logic slot index given from the part before the Flick slots and remove the used minislots
   * (Note that we always have a negative offset of TSM_DEFAULT_MINISLOTS_GROUPING - 1)
   * 5. If we are further than the bootstrap section then we at least scheduled a Flick slot so we can remove the corresponding minislots and add the corresponding logic slots
   * 6. After this crystal becomes regular with a period of TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + ntx_T + SINK_RADIUS + ntx_A) + FS_MINISLOT minislots corresponding to 
   *    SINK_RADIUS + ntx_T + SINK_RADIUS + ntx_A + FS_MACROSLOT logic slots. We can then calculate how many whole groups we have and then handle the remainder
   */
  uint32_t res = 0;

  res += MIN(minislots, TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S)) / TSM_DEFAULT_MINISLOTS_GROUPING;

  if (minislots < TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S)) {
    return res;
  }

  minislots -= TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S);
  res+= FS_MACROSLOT;

  if (minislots < FS_MINISLOT) {
    return res;
  }

  minislots -= FS_MINISLOT;

  uint32_t cycles = minislots / (TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) + FS_MINISLOT);

  res += cycles * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A + FS_MACROSLOT);
  minislots -= cycles * (TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) + FS_MINISLOT);

  res += MIN(minislots, TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A)) / TSM_DEFAULT_MINISLOTS_GROUPING;

  if (minislots < TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A)) {
    return res;
  }

  minislots -= TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A);
  res += FS_MACROSLOT;

  if (minislots < FS_MINISLOT) {
    return res;
  } else {
    ERR("This should not be possible");
    return 0;
  }
#endif
}

char crystal_peer_scan_thread() {
  static uint16_t termination_counter;
  static uint16_t termination_cap;

  static uint32_t original_preamble;

  PT_BEGIN(&crystal_scan_pt);

  termination_counter = 0;
  termination_cap = CRYSTAL_SCAN_TERMINATION;

  // Save the original preambleto so that if we increase it to take into account the jitter we can then restore it
  original_preamble = tsm_get_default_preambleto();

  tsm_set_default_preambleto(original_preamble + ((GLOSSY_MAX_JITTER_MULT > 0)? (GLOSSY_JITTER_STEP * GLOSSY_MAX_JITTER_MULT) : 0)); // guards

  tsm_next_action.rx_guard_time = (1000 * UUS_TO_DWT_TIME_32); // start some time earlier than the sink

  while (1) { // Bootstrap while

    /* If are synchronized enough (i.e. we did not lose too many consecutive bootstrap, see N_SILENT_EPOCHS_TO_STOP_SENDING)
     * use a RX slot (which consumes less energy but requires synchronization)
     * otherwise use a SCAN slot (which consumes more power but does not require synchronization)
     */
    if (crystal_context.cumulative_failed_synchronizations < N_SILENT_EPOCHS_TO_STOP_SENDING) {
      TSM_RX_SLOT(&crystal_scan_pt, buf.raw);
      termination_counter += 1;
    } else {
      TSM_SCAN(&crystal_scan_pt, buf.raw);
    }

    if (tsm_prev_action.status == TREX_RX_SUCCESS) {
      if ((buf.type == CRYSTAL_TYPE_SYNC && tsm_prev_action.payload_len == CRYSTAL_S_HDR_LEN + crystal_conf.plds_S) ||
          (buf.type == CRYSTAL_TYPE_ACK && tsm_prev_action.payload_len == CRYSTAL_A_HDR_LEN + crystal_conf.plds_A)) {
        // Regardless of being a SYNC or ACK packet get the epoch number
        crystal_epoch_t rcvdEpoch = (buf.type == CRYSTAL_TYPE_SYNC)? buf.sync_hdr.epoch : buf.ack_hdr.epoch;

        // NOTE: Slightly different from original crystal as it enforce ever-increasing epoch count
        if (rcvdEpoch > crystal_context.epoch) {
          tsm_next_action.accept_sync = 1;

          /*
           * If the received epoch is probably an error (i.e. with a difference from the current one of more than 50),
           * just use the epoch that we expect (current+1)
           */
          if (rcvdEpoch > crystal_context.epoch + 50) {
            rcvdEpoch = crystal_context.epoch + 1;
          }

          crystal_context.epoch = rcvdEpoch;

          crystal_info.epoch = crystal_context.epoch;
          logging_context = crystal_context.epoch;

          if (buf.type == CRYSTAL_TYPE_ACK) {
            // This is not really needed as it will be overwritten anyways but it might be useful for debugging
            crystal_context.last_ack_flags = buf.ack_hdr.flags;
          }

          PRINT("epoch %hu", crystal_context.epoch);

          // Exit (cumulative_failed_synchronization is reset later)
          break;
        } else {
            PRINT("leak %hu %hu", crystal_context.epoch, rcvdEpoch);
            continue;
        }
      } else if (buf.type == CRYSTAL_TYPE_DATA) {
        continue;
      }
    }

    if (termination_counter >= termination_cap) {
      break;
    }
  } // Bootstrap while

  /*
   * If we lost the bootstrap increase the epoch by 1 and increase the number of cumulative_failed_synchronization
   * otherwise just reset the number of cumulative_failed_synchronization as we received the bootstrap
   */
  if (tsm_prev_action.status != TREX_RX_SUCCESS && termination_counter >= termination_cap) {
    crystal_context.cumulative_failed_synchronizations += 1;
    crystal_context.cumulative_failed_synchronizations = MIN(crystal_context.cumulative_failed_synchronizations, N_SILENT_EPOCHS_TO_STOP_SENDING);

    ++crystal_context.epoch;
    crystal_info.epoch = crystal_context.epoch;
    logging_context = crystal_context.epoch;
  } else {
    // If we are here then we successfully synchronized, thus we should reset the failed synchronizations counter
    crystal_context.cumulative_failed_synchronizations = 0;
  }

  // Reset the preambleto to its original value
  tsm_set_default_preambleto(original_preamble);

  PT_END(&crystal_scan_pt);
}

char crystal_peer_thread() {
  static uint8_t* payload;

  static bool i_tx;
  static uint16_t correct_packet;

  static bool sleep_order;

  static uint16_t n_empty_ts;
  static uint16_t n_radio_reception_errors;

  static uint16_t n_noacks;
  static uint16_t n_bad_acks;
  static uint16_t n_all_acks;

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
  static bool first_fs;
#endif

  PT_BEGIN(&crystal_pt);

  // There is no initial synch in this version of crystal (differently from crystal.c) so there is no
  // need to send a value different from success (note that in our example app is not used anyway)
  app_crystal_start_done(true);

  while (1) { // Epoch while

    payload = NULL;
    i_tx = false;
    correct_packet = 0;
    sleep_order = false;

    n_empty_ts = 0;
    n_noacks = 0;
    n_bad_acks = 0;
    n_all_acks = 0;

    crystal_context.received_bitmap = 0x0;
    crystal_context.ack_bitmap = 0x0;

#if CRYSTAL_SYNC_ACKS
    crystal_context.synced_with_ack = false;
#endif

    STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
    if (crystal_context.epoch < 20) {
      // Do not consider statetime in the first epochs but wait some time so that there is enough stable data to estimate the ratio between radio and cpu clock
      STATETIME_MONITOR(dw1000_statetime_stop());
    }

    if (crystal_context.epoch > 1) {
      // Do not run app_pre_epoch (only needed in between epochs) in the first epoch
      app_pre_epoch();
    }

    app_pre_S();

    /*
     * For the S phase we first do a scan
     */
    PT_SPAWN(&crystal_pt, &crystal_scan_pt, crystal_peer_scan_thread());

    if (tsm_prev_action.logic_slot_idx >= SINK_RADIUS + crystal_conf.ntx_S) {
      PRINT("Skipped bootstrap");
    }

    correct_packet = 0;

    if (crystal_context.cumulative_failed_synchronizations == 0) {
      correct_packet = (buf.type == CRYSTAL_TYPE_SYNC && tsm_prev_action.payload_len == CRYSTAL_S_HDR_LEN + crystal_conf.plds_S);

      // If this is a correct bootstrap (i.e. if we did receive a correct synchronization and not a late synchronization)
      // set everything and propagate the bootstrap message
      if (correct_packet && (tsm_prev_action.logic_slot_idx + 1 < SINK_RADIUS + crystal_conf.ntx_S)) {
        GLOSSY_TX(&crystal_pt,
                  &glossy_pt,
                  SINK_RADIUS + crystal_conf.ntx_S - tsm_prev_action.logic_slot_idx - 1,
                  crystal_conf.ntx_S,
                  buf.raw,
                  crystal_conf.plds_S + CRYSTAL_S_HDR_LEN,
                  false);
      }
    }

    app_post_S(correct_packet, buf.raw + TSM_HDR_LEN + CRYSTAL_S_HDR_LEN);
    BZERO_BUF();

    // If a am "synchronized enough", regardless of having received a bootstrap, an ACK or nothing, start with the protocol
    if (crystal_context.cumulative_failed_synchronizations < N_SILENT_EPOCHS_TO_STOP_SENDING) {

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
      // Regardless of having received a bootstrap, an ACK or nothing, if I am synchronized enough and in time, use Flick
      if (tsm_prev_action.logic_slot_idx < SINK_RADIUS + crystal_conf.ntx_S) {
        tsm_next_action.progress_logic_slots = SINK_RADIUS + crystal_conf.ntx_S - tsm_prev_action.logic_slot_idx;
        tsm_next_action.progress_minislots = TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S) + TSM_DEFAULT_MINISLOTS_GROUPING - 1 - tsm_prev_action.minislot_idx;

        if (app_is_originator()) {
          TSM_TX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

          crystal_context.last_fs = true;
        } else {
          tsm_next_action.max_fs_flood_duration = MAX_LATENCY_FS;
          tsm_next_action.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
          TSM_RX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

          if (tsm_prev_action.status == TREX_FS_EMPTY) {
            // Should stop and go to the next epoch
            PRINT("Nothing responded to Flick, restart");
            crystal_context.last_fs = false;
          } else if (tsm_prev_action.status == TREX_FS_ERROR) {
            ERR("Unexpected result from sync Flick %hu", tsm_prev_action.status);
            crystal_context.last_fs = false;
          } else {
            crystal_context.last_fs = true;
          }
        }
      }
#endif

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
      first_fs = crystal_context.last_fs;

      if (first_fs) {
#endif
        if (app_is_originator()){
          PRINT("IS ORIG %hu", crystal_context.epoch);
        }

        while(1) { // For each TA
          payload = app_pre_T();

          if (payload != NULL) {
            crystal_context.received_bitmap = ack_node(crystal_context.received_bitmap, node_id);
          }

          i_tx = (payload != NULL && (
                crystal_context.cumulative_failed_synchronizations < N_SILENT_EPOCHS_TO_STOP_SENDING 
#if CRYSTAL_SYNC_ACKS
                || crystal_context.n_noack_epochs < N_SILENT_EPOCHS_TO_STOP_SENDING
#endif
          ));

          correct_packet = 0;

          // Calculate how many minislots and logical slots we have to progress to start the next slot at the beginning of the next TA
#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
          tsm_next_action.progress_logic_slots =
            SINK_RADIUS + crystal_conf.ntx_S +
            (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A)
            - tsm_prev_action.logic_slot_idx;

          tsm_next_action.progress_minislots =
            TSM_DEFAULT_MINISLOTS_GROUPING * (
                SINK_RADIUS + crystal_conf.ntx_S +
                (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A))
            + TSM_DEFAULT_MINISLOTS_GROUPING - 1
            - tsm_prev_action.minislot_idx;
#else
          tsm_next_action.progress_logic_slots =
            SINK_RADIUS + crystal_conf.ntx_S + FS_MACROSLOT +
            (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A + FS_MACROSLOT)
            - tsm_prev_action.logic_slot_idx;

          tsm_next_action.progress_minislots = 
            TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S)
            + FS_MINISLOT
            + (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (
                TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A)
                + FS_MINISLOT)
            + TSM_DEFAULT_MINISLOTS_GROUPING - 1
            - tsm_prev_action.minislot_idx;
#endif

          /*
           * If we decided to transmit in this TA prepare the packet and use the glossy layer to transmit.
           * Otherwise listen for the duration of the glossy flood 
           */
          if (i_tx) {
            // Set the type of the packet to data and insert as originator this node
            buf.type = CRYSTAL_TYPE_DATA;
            buf.data_hdr.src = node_id;

            memcpy(buf.raw + TSM_HDR_LEN + CRYSTAL_T_HDR_LEN, payload, crystal_conf.plds_T);

            GLOSSY_TX(&crystal_pt,
                      &glossy_pt,
                      SINK_RADIUS + crystal_conf.ntx_T,
                      crystal_conf.ntx_T,
                      buf.raw,
                      crystal_conf.plds_T + CRYSTAL_T_HDR_LEN,
                      true);
          } else {
            GLOSSY_RX(&crystal_pt,
                      &glossy_pt,
                      SINK_RADIUS + crystal_conf.ntx_T,
                      crystal_conf.ntx_T,
                      buf.raw,
                      false,
                      true);

            if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
              correct_packet = (glossy_context.received_len == CRYSTAL_T_HDR_LEN + crystal_conf.plds_T) && buf.type == CRYSTAL_TYPE_DATA;
              n_empty_ts = 0;

              crystal_context.received_bitmap = ack_node(crystal_context.received_bitmap, buf.data_hdr.src);
            } else {
              ++n_empty_ts;
            }
          }

          app_between_TA(correct_packet,
                         buf.raw + TSM_HDR_LEN + CRYSTAL_T_HDR_LEN,
                         buf.raw + TSM_HDR_LEN);
          BZERO_BUF();

          correct_packet = 0;

          GLOSSY_RX(&crystal_pt,
                    &glossy_pt,
                    SINK_RADIUS + crystal_conf.ntx_A,
                    crystal_conf.ntx_A,
                    buf.raw,
                    CRYSTAL_SYNC_ACKS,
                    false);

          if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
            correct_packet = (glossy_context.received_len == crystal_conf.plds_A + CRYSTAL_A_HDR_LEN) && 
                             buf.type == CRYSTAL_TYPE_ACK &&
                             buf.ack_hdr.epoch >= crystal_context.epoch;

            if (correct_packet) {
              n_noacks = 0;
              n_bad_acks = 0;
              n_all_acks ++;

              if (buf.ack_hdr.epoch > crystal_context.epoch) {
                crystal_context.epoch = buf.ack_hdr.epoch;
                crystal_info.epoch = crystal_context.epoch;
              }

#if CRYSTAL_SYNC_ACKS
              // NOTE: Differently from original crystal we do not allow for "good" receptions with leading edge detection so if 
              crystal_context.synced_with_ack ++;
              crystal_context.n_noack_epochs = 0; // it's important to reset it here to reenable TX right away (if it was suppressed)
#endif

              if (buf.ack_hdr.ack_bitmap == ~((uint64_t)0)) {
                sleep_order = true;
              }

              // Merge the bitmap that we received with the bitmap we accumulated
              crystal_context.ack_bitmap |= buf.ack_hdr.ack_bitmap;
            } else {
              /*
               * Received something but not an ack.
               * As we still received something we increase only n_bad_acks and not n_noacks (giving an additional chance if we are out of sync)
               */
              n_bad_acks ++;
            }

            n_radio_reception_errors = 0;
          } else {
              if (crystal_conf.xa == 0)
                  n_noacks ++; // no "noise detection"
              else if (glossy_context.rx_status == GLOSSY_RX_ERROR) {
                  n_radio_reception_errors ++;

                  if (n_radio_reception_errors > crystal_conf.xa)
                      n_noacks ++;
              }
              else {
                  n_noacks ++;
                  n_radio_reception_errors = 0;
              }
          }

          app_post_A(correct_packet, buf.raw + TSM_HDR_LEN + CRYSTAL_A_HDR_LEN, &buf.ack_hdr);
          // If we received the data use it to set the flags otherwise reset them
          crystal_context.last_ack_flags = correct_packet ? buf.ack_hdr.flags : 0;
          BZERO_BUF();

#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
          // -- Phase A end
          // shall we stop?
          if (sleep_order || (get_n_ta(tsm_prev_action.logic_slot_idx)+1 >= CRYSTAL_MAX_TAS) || // always stop when ordered or max is reached
                    (
                     ( payload != NULL  && (n_noacks >= crystal_conf.z)) ||
                     ((payload == NULL) && (n_noacks >= crystal_conf.y) && n_empty_ts >= crystal_conf.y)
                    )
              ) {
               break; // Stop the TA chain
          }
#else // CRYSTAL_VARIANT == CRYSTAL_VARIANT_SIMPLE
          if (app_has_packet() || (correct_packet && (crystal_context.received_bitmap & ~crystal_context.ack_bitmap))) {
            TSM_TX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

            crystal_context.last_fs = true;
          } else {
            tsm_next_action.max_fs_flood_duration = MAX_LATENCY_FS;
            tsm_next_action.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
            TSM_RX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

            if (tsm_prev_action.status == TREX_FS_EMPTY) {
              // Should stop and go to the next epoch
              crystal_context.last_fs = false;
            } else if (tsm_prev_action.status == TREX_FS_ERROR) {
              ERR("Unexpected result from sync Flick %hu", tsm_prev_action.status);
              crystal_context.last_fs = false;
            } else {
              crystal_context.last_fs = true;
            }
          }

          // If we received an ACK, and the flick slot is negative stop
          if (sleep_order || (get_n_ta(tsm_prev_action.slot_idx)+1 >= CRYSTAL_MAX_TAS)) {
            if (sleep_order) {
              PRINT("Exit epoch %hu due sleep order", crystal_context.epoch);
            } else {
              PRINT("Exit epoch %hu due max TAs", crystal_context.epoch);
            }
            break;
          } else if (!crystal_context.last_fs) {
            PRINT("Exit epoch %hu due to negative Flick", crystal_context.epoch);
            break;
          } else if (payload != NULL && n_noacks >= crystal_conf.z) {
            // If we have data to transmit after a certain amount of missed (N)ACKs just give up 
            // (otherwise we will continue to use flick on the network when it is broken with the result that no-one will ever go to sleep)
            PRINT("Exit epoch %hu due missed consecutive acks", crystal_context.epoch);
            break;
          }
#endif
        }
#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
      }
#endif

#if CRYSTAL_SYNC_ACKS
      if (
#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
          first_fs &&
#endif
          !crystal_context.synced_with_ack) {
        crystal_context.n_noack_epochs++;
      }
#endif
    }

    PRINT("E %hu NSLOTS %lu", crystal_context.epoch, tsm_prev_action.minislot_idx < 0 ? 0 : tsm_prev_action.minislot_idx + 1);

    logging_context = crystal_context.epoch;

    STATETIME_MONITOR(dw1000_statetime_stop(); printf("STATETIME "); dw1000_statetime_print());

    app_epoch_end();

    TSM_RESTART(&crystal_pt, crystal_conf.period);
  } // Epoch while

  PT_END(&crystal_pt);
}

char crystal_sink_thread() {
  static uint8_t * payload;

  static uint16_t correct_packet;
  static uint8_t n_radio_reception_errors;
  static uint8_t n_empty_ts;

  static bool sleep_order;

  static uint32_t last_new_pkt_rx;

  PT_BEGIN(&crystal_pt);

  app_crystal_start_done(true);

  while (1) { // Epoch while
    if (crystal_context.epoch > 1) {
      // Do not run app_pre_epoch (only needed in between epochs) in the first epoch
      app_pre_epoch();
    }

    payload = NULL;

    correct_packet = 0;
    n_radio_reception_errors = 0;
    n_empty_ts = 0;
    sleep_order = false;

    crystal_context.received_bitmap = 0x0;
    crystal_context.ack_bitmap = 0x0;

    last_new_pkt_rx = 0;

#if CRYSTAL_SYNC_ACKS
    crystal_context.synced_with_ack = false;
#endif

    STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
    if (crystal_context.epoch < 20) {
      // Do not consider statetime in the first epochs but wait some time so that there is enough stable data to estimate the ratio between radio and cpu clock
      STATETIME_MONITOR(dw1000_statetime_stop());
    }

    crystal_context.epoch++;
    crystal_info.epoch = crystal_context.epoch;

    // === S phase
    // NOTE: Using the payload given by this call seems necessary only for the sink
    payload = app_pre_S();

    buf.type = CRYSTAL_TYPE_SYNC;
    buf.sync_hdr.epoch = crystal_context.epoch;

    if (payload != NULL) {
      memcpy(buf.raw + TSM_HDR_LEN + CRYSTAL_S_HDR_LEN, payload, crystal_conf.plds_S);
    }

    GLOSSY_TX(&crystal_pt,
              &glossy_pt,
              SINK_RADIUS + crystal_conf.ntx_S,
              crystal_conf.ntx_S,
              buf.raw,
              crystal_conf.plds_S + CRYSTAL_S_HDR_LEN,
              false);

    app_post_S(0, NULL);
    BZERO_BUF();

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
    // Advance until we reach the point at which we can schedule the Flick slot
    tsm_next_action.progress_logic_slots = SINK_RADIUS + crystal_conf.ntx_S - tsm_prev_action.logic_slot_idx;
    tsm_next_action.progress_minislots = TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S) + TSM_DEFAULT_MINISLOTS_GROUPING - 1 - tsm_prev_action.minislot_idx;

    tsm_next_action.max_fs_flood_duration = MAX_LATENCY_FS;
    tsm_next_action.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
    TSM_RX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

    if (tsm_prev_action.status == TREX_FS_EMPTY) {
      // Should stop and go to the next epoch
      PRINT("Nothing responded to Flick, restart");
      crystal_context.last_fs = false;
    } else if (tsm_prev_action.status == TREX_FS_ERROR) {
      ERR("Unexpected result from sync Flick %hu", tsm_prev_action.status);
      crystal_context.last_fs = false;
    } else {
      crystal_context.last_fs = true;
    }
#endif

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
    if (crystal_context.last_fs) {
#endif

      // TODO: Check if while condition with +1 is correct
      while (!sleep_order && get_n_ta(tsm_prev_action.logic_slot_idx) + 1 < CRYSTAL_MAX_TAS) {
        // === T phase
        app_pre_T();

        // Advance to reach the next TA
#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
          tsm_next_action.progress_logic_slots = 
            SINK_RADIUS + crystal_conf.ntx_S 
            + (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) 
            - tsm_prev_action.logic_slot_idx;

          tsm_next_action.progress_minislots = 
            TSM_DEFAULT_MINISLOTS_GROUPING * (
              SINK_RADIUS + crystal_conf.ntx_S 
              + (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A))
            + TSM_DEFAULT_MINISLOTS_GROUPING - 1
            - tsm_prev_action.minislot_idx;

#else
          tsm_next_action.progress_logic_slots = 
            SINK_RADIUS + crystal_conf.ntx_S 
            + FS_MACROSLOT 
            + (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A + FS_MACROSLOT) 
            - tsm_prev_action.logic_slot_idx;

          tsm_next_action.progress_minislots = 
            TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_S)
            + FS_MINISLOT 
            + (get_n_ta(tsm_prev_action.logic_slot_idx) + 1) * (
                TSM_DEFAULT_MINISLOTS_GROUPING * (SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) 
                + FS_MINISLOT) 
            + TSM_DEFAULT_MINISLOTS_GROUPING - 1
            - tsm_prev_action.minislot_idx;

#endif

        GLOSSY_RX(&crystal_pt,
                  &glossy_pt,
                  SINK_RADIUS + crystal_conf.ntx_T,
                  crystal_conf.ntx_T,
                  buf.raw,
                  false,
                  true);

        // Reset the ack flags and then set them depending on the rest of the glossy flood
        crystal_context.last_ack_flags = 0;

        if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
          /*
           * Note that regardless of receiving an already received packet or a new one,
           * we send an ACK as soon as we receive something
           */
          crystal_context.last_ack_flags |= CRYSTAL_ACK_MASK;
        } else {
          crystal_context.last_ack_flags |= CRYSTAL_NACK_MASK;
        }

        correct_packet = 0;
        if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
          correct_packet = (buf.type == CRYSTAL_TYPE_DATA && glossy_context.received_len == CRYSTAL_T_HDR_LEN + crystal_conf.plds_T);

          // As we received some data reset the cumulative counters for empty ts and reception errors
          n_radio_reception_errors = 0;
          n_empty_ts = 0;

          // If what we received created any change consider this reception for the last new received packet
          if (crystal_context.received_bitmap != ack_node(crystal_context.received_bitmap, buf.data_hdr.src)) {
            last_new_pkt_rx = glossy_tsm_minislot_last_rx;

            if (glossy_tsm_minislot_last_rx == 0) {
              ERR("Received glossy flood in minislot 0");
            }
          }

          crystal_context.received_bitmap = ack_node(crystal_context.received_bitmap, buf.data_hdr.src);
          crystal_context.ack_bitmap = ack_node(crystal_context.ack_bitmap, buf.data_hdr.src);
        } else if (crystal_conf.x > 0 && glossy_context.rx_status == GLOSSY_RX_ERROR) {
          ++n_radio_reception_errors;
        } else { // Complete silence
          n_radio_reception_errors = 0;
          ++n_empty_ts;
        }

        // === A phase
        payload = app_between_TA(correct_packet, buf.raw + TSM_HDR_LEN + CRYSTAL_T_HDR_LEN, &buf.data_hdr);
        BZERO_BUF();

#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
        sleep_order = (
              (get_n_ta(tsm_prev_action.logic_slot_idx) >= CRYSTAL_MAX_TAS - 1) ||
              (n_empty_ts >= CRYSTAL_SINK_MAX_EMPTY_TS_DYNAMIC(get_n_ta(tsm_prev_action.logic_slot_idx))) ||
              (crystal_conf.x && n_radio_reception_errors >= crystal_conf.x)
              );

        if (sleep_order && (crystal_conf.x && n_radio_reception_errors >= crystal_conf.x)) {
          PRINT("Exit (sink) epoch %hu due max reception errors", crystal_context.epoch);
        }

        if (sleep_order && (n_empty_ts >= CRYSTAL_SINK_MAX_EMPTY_TS_DYNAMIC(get_n_ta(tsm_prev_action.logic_slot_idx)))) {
          PRINT("Exit (sink) epoch %hu due max empty ts", crystal_context.epoch);
        }

        if (sleep_order && (get_n_ta(tsm_prev_action.logic_slot_idx) >= CRYSTAL_MAX_TAS - 1)) {
          PRINT("Exit (sink) epoch %hu due max TAs", crystal_context.epoch);
        }
#else // CRYSTAL_VARIANT == CRYSTAL_VARIANT_SIMPLE
        sleep_order = (
              (get_n_ta(tsm_prev_action.logic_slot_idx) >= CRYSTAL_MAX_TAS - 1) ||
              (crystal_conf.x && n_radio_reception_errors >= crystal_conf.x)
              );

        if (sleep_order && (crystal_conf.x && n_radio_reception_errors >= crystal_conf.x)) {
          PRINT("Exit (sink) epoch %hu due max reception errors", crystal_context.epoch);
        }

        if (sleep_order && (get_n_ta(tsm_prev_action.logic_slot_idx) >= CRYSTAL_MAX_TAS - 1)) {
          PRINT("Exit (sink) epoch %hu due max TAs", crystal_context.epoch);
        }
#endif

        // Set the ack packet
        buf.type = CRYSTAL_TYPE_ACK;
        buf.ack_hdr.epoch = crystal_context.epoch;
        buf.ack_hdr.flags = crystal_context.last_ack_flags;
        buf.ack_hdr.ack_bitmap = crystal_context.ack_bitmap;

        // If we decided to put the entire network to sleep put special values as the bitmap
        if (sleep_order) {
          buf.ack_hdr.ack_bitmap = ~((uint64_t)0);
        }

        WARNIF(payload == NULL);

        if (payload != NULL) {
          memcpy(buf.raw + TSM_HDR_LEN + CRYSTAL_A_HDR_LEN, payload, crystal_conf.plds_A);
        }

        GLOSSY_TX(&crystal_pt,
                  &glossy_pt,
                  SINK_RADIUS + crystal_conf.ntx_A,
                  crystal_conf.ntx_A,
                  buf.raw,
                  crystal_conf.plds_A + CRYSTAL_A_HDR_LEN,
                  false);

        app_post_A(0, buf.raw + TSM_HDR_LEN + CRYSTAL_A_HDR_LEN, buf.raw + TSM_HDR_LEN);
        BZERO_BUF();

#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_SIMPLE
        tsm_next_action.max_fs_flood_duration = MAX_LATENCY_FS;
        tsm_next_action.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
        TSM_RX_FS_SLOT(&crystal_pt, FS_MACROSLOT, FS_MINISLOT);

        if (tsm_prev_action.status == TREX_FS_EMPTY) {
          // Should stop and go to the next epoch
          crystal_context.last_fs = false;
        } else if (tsm_prev_action.status == TREX_FS_ERROR) {
          ERR("Unexpected result from sync Flick %hu", tsm_prev_action.status);
          crystal_context.last_fs = false;
        } else {
          crystal_context.last_fs = true;
        }

        if (sleep_order || (get_n_ta(tsm_prev_action.logic_slot_idx)+1 >= CRYSTAL_MAX_TAS)) {
          PRINT("Exit (sink) epoch %hu due max TAs", crystal_context.epoch);
          break;
        } else if (!crystal_context.last_fs) {
          PRINT("Exit (sink) epoch %hu due negative Flick", crystal_context.epoch);
          break;
        }
#endif
      }
#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
    }
#endif

    PRINT("E %hu NSLOTS %lu", crystal_context.epoch, tsm_prev_action.minislot_idx < 0 ? 0 : tsm_prev_action.minislot_idx + 1);

    if (last_new_pkt_rx != 0) {
      PRINT("E %hu LAST RX %lu", crystal_context.epoch, last_new_pkt_rx);
    }

    logging_context = crystal_context.epoch;

    STATETIME_MONITOR(dw1000_statetime_stop(); printf("STATETIME "); dw1000_statetime_print());
    app_epoch_end();

    TSM_RESTART(&crystal_pt, crystal_conf.period);

  } // Epoch while

  PT_END(&crystal_pt);
}

void crystal_init() {
  tsm_init();
  map_nodes();

  print_nodes();

#if STATETIME_CONF_ON
  STATETIME_MONITOR(dw1000_statetime_context_init());
#endif
}

bool crystal_start(const crystal_config_t* conf) {
  printf("Starting Crystal\n");
  // check the config
  if (CRYSTAL_S_HDR_LEN + conf->plds_S > CRYSTAL_PKTBUF_LEN) {
    printf("Wrong S len config!\n");
    return false;
  } else if (CRYSTAL_T_HDR_LEN + conf->plds_T > CRYSTAL_PKTBUF_LEN) {
    printf("Wrong T len config!\n");
    return false;
  } else if (CRYSTAL_A_HDR_LEN + conf->plds_A > CRYSTAL_PKTBUF_LEN) {
    printf("Wrong A len config!\n");
    return false;
  } else if (conf->period == 0) {
    printf("Period cannot be zero!\n");
    return false;
  } else if (conf->period > CRYSTAL_MAX_PERIOD) {
    printf("Period greater than max period!\n");
    return false;
  } else if (conf->scan_duration == 0) {
    printf("Scan duration cannot be zero!\n");
    return false;
  } else if (conf->scan_duration > CRYSTAL_MAX_SCAN_EPOCHS) {
    printf("Scan duration cannot be greater than scan epochs!\n");
    return false;
  }

  crystal_context.epoch = 0;
  crystal_context.cumulative_failed_synchronizations = N_SILENT_EPOCHS_TO_STOP_SENDING;

#if CRYSTAL_SYNC_ACKS
  crystal_context.n_noack_epochs = 0;
  crystal_context.synced_with_ack = 0;
#endif

  crystal_info.epoch = 0;

  crystal_conf = *conf;

  uint32_t tsm_slot_duration = SLOT_DURATION;
  uint32_t tsm_rx_timeout = TIMEOUT;

#if CRYSTAL_VARIANT == CRYSTAL_VARIANT_NO_FS
  /*
   * 1. Calculate the number of slots available in the period
   * 2. Subtract the bootstrap phase and the first TA
   * 3. Divide the remaining slots for the number of slots needed for a TA
   */
  crystal_max_tas_dyn = ((crystal_conf.period / tsm_slot_duration) - TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + crystal_conf.ntx_S + SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A))
    / (TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A));


#else
  /*
   * 1. Calculate the number of slots available in the period
   * 2. Subtract the bootstrap phase and the first TA
   * 3. Divide the remaining slots for the number of slots needed for a TA
   */
  crystal_max_tas_dyn = ((crystal_conf.period / tsm_slot_duration) - (TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + crystal_conf.ntx_S + FS_MACROSLOT + SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) + FS_MINISLOT)
                        ) / (TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + crystal_conf.ntx_S + FS_MACROSLOT + SINK_RADIUS + crystal_conf.ntx_T + SINK_RADIUS + crystal_conf.ntx_A) + FS_MINISLOT);

#endif

  tsm_set_default_preambleto(32*PRE_SYM_PRF64_TO_DWT_TIME_32);

  PRINT("RXTO %"PRIu32, tsm_rx_timeout);
  PRINT("SLOT_DURATION %"PRIu32, tsm_slot_duration);
  PRINT("PERIOD %"PRIu32, crystal_conf.period);
  PRINT("MAX TAS %"PRIu32, crystal_max_tas_dyn);

  // NOTE: We assume that the there was already a delay for the nodes to be completely up in the application code
  if (conf->is_sink) {
    PRINT("IS_SINK");
    tsm_minislot_start(tsm_slot_duration, tsm_rx_timeout, (tsm_slot_cb)crystal_sink_thread, &from_minislots_to_logic_slots);
  } else {
    tsm_minislot_start(tsm_slot_duration, tsm_rx_timeout, (tsm_slot_cb)crystal_peer_thread, &from_minislots_to_logic_slots);
  }

  return true;
}
