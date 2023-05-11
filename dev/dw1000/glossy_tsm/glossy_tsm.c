#include "glossy_tsm.h"

#include <stdint.h>
#include <inttypes.h>

#include "trex-tsm.h"
#include "trex.h"

#include "lib/random.h"

#include "dw1000-conv.h"
#include "print-def.h"

#define LOG_PREFIX "gt"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#pragma message STRDEF(GLOSSY_LATENCY_LOG)
#pragma message STRDEF(GLOSSY_MAX_JITTER_MULT)
#pragma message STRDEF(GLOSSY_JITTER_STEP)

struct glossy_context_t glossy_context;
struct pt glossy_pt;

uint32_t glossy_tsm_minislot_last_rx = 0;

void reset_context() {
  glossy_context.rx_status    = GLOSSY_RX_UNINITIALIZED;
  glossy_context.received_len = 0;
  glossy_tsm_minislot_last_rx = 0;
}

char glossy_trx();

#ifndef GLOSSY_LATENCY_LOG
#define GLOSSY_LATENCY_LOG 0
#endif

#if GLOSSY_LATENCY_LOG
#include "dw1000-statetime.h"
#include "deca_device_api.h"
#endif

static uint32_t elapsed;
struct glossy_next_action_t glossy_next_action;

char glossy_trx() {

  PT_BEGIN(&glossy_pt);

#if GLOSSY_LATENCY_LOG
  glossy_context.start_4ns = 0;
  glossy_context.first_rx = true;
#endif

  WARNIF(glossy_next_action.max_len == 0);
  WARNIF(glossy_next_action.N == 0);
  WARNIF(glossy_next_action.buffer == NULL);

  if (!glossy_next_action.is_rx) {
    WARNIF(glossy_next_action.data_len == 0);
  }

  glossy_context.original_preamble = tsm_get_default_preambleto();

  // Change the preambleto to take into account the additional jitter (enabled when GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter)
  tsm_set_default_preambleto(glossy_context.original_preamble + ((GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter)? (GLOSSY_JITTER_STEP * GLOSSY_MAX_JITTER_MULT) : 0)); // add jitter to preambleto if present

  reset_context();

  elapsed = -1;

  // NOTE: We do not modify progress_slots as this is part of the interface to interact with glossy_tsm

  glossy_context.n_tx = 0;
  glossy_context.logic_deadline = tsm_prev_action.logic_slot_idx + tsm_next_action.progress_logic_slots + glossy_next_action.max_len;
  glossy_context.deadline = tsm_prev_action.minislot_idx + tsm_next_action.progress_minislots + (glossy_next_action.max_len * TSM_DEFAULT_MINISLOTS_GROUPING);

  if (glossy_next_action.max_len == 0) {
    WARN("mi %" PRIu32 " pms %" PRIu32 " dl %" PRIu32, tsm_prev_action.minislot_idx, tsm_next_action.progress_minislots, glossy_context.deadline);
  }

  if (glossy_next_action.is_rx) {
    // Try to receive something until the next slot is after at or past deadline
    while (tsm_prev_action.minislot_idx + tsm_next_action.progress_minislots < glossy_context.deadline) {
      TSM_RX_SLOT(&glossy_pt, glossy_next_action.buffer);

#if GLOSSY_LATENCY_LOG
      if (glossy_context.first_rx) {
        /*
         * If this is the first reception slot, save the time at which the reception started, this 
         * will be used later to obtain the latency of the glossy flood
         */
        glossy_context.start_4ns = dw1000_statetime_get_schedule_32hi();
        glossy_context.first_rx = false;
      }
#endif

      if (tsm_prev_action.status == TREX_RX_SUCCESS) {
        glossy_context.rx_status = GLOSSY_RX_SUCCESS;

        glossy_context.received_len = tsm_prev_action.payload_len;

        if (tsm_prev_action.payload_len == 0) {
          WARN("Received null packet at %" PRIu32, tsm_prev_action.logic_slot_idx);
        }

        if (glossy_next_action.update_tref) {
          tsm_next_action.accept_sync = 1;
        }

#if GLOSSY_LATENCY_LOG
        /*
         * If the options GLOSSY_LATENCY_LOG is enabled, obtain the difference in time between the start of the first glossy
         * flood and the RMARKER of the received packet. Afterwards convert it to ns and add to it the rest of the packet 
         * (estimated from the payload length and radio configuration)
         */
        elapsed = dwt_readrxtimestamphi32();
        elapsed -= glossy_context.start_4ns;

        elapsed = ((uint32_t)(elapsed*DWT_TICK_TO_NS_32));

        elapsed += estimate_payload_time_ns(glossy_context.received_len + TSM_HDR_LEN + 2); // The amount we received + the TREXD_FRAME_OVERHEAD (due 2 byte CRC)
#endif
        glossy_tsm_minislot_last_rx = tsm_prev_action.minislot_idx;

        break;
      } else if (tsm_prev_action.status == TREX_RX_ERROR) {
        if (glossy_context.rx_status != GLOSSY_RX_SUCCESS) {
          glossy_context.rx_status = GLOSSY_RX_ERROR;
        }
      } else if (tsm_prev_action.status == TREX_RX_TIMEOUT) {
        if (glossy_context.rx_status == GLOSSY_RX_UNINITIALIZED) {
          glossy_context.rx_status = GLOSSY_RX_TIMEOUT;
        }
      }
    }
  }

  /*
   * If this is a transmission(!is_rx) or we received with success (and thus we have to re-propagate the message)
   * re-transmit at max N times within the allotted time for the glossy flood
   */
  if (!glossy_next_action.is_rx || (glossy_context.rx_status == GLOSSY_RX_SUCCESS)) {
    if (glossy_next_action.is_rx && glossy_context.received_len == 0) {
      WARN("is_rx && data_len == 0 at %" PRIu32 " (%" PRIu32 ")with rx_status %hu", tsm_prev_action.logic_slot_idx, tsm_prev_action.minislot_idx, glossy_context.rx_status);
    }

    while ((glossy_context.n_tx < glossy_next_action.N) &&
           (tsm_prev_action.minislot_idx + tsm_next_action.progress_minislots < glossy_context.deadline)) { // Do all the re-transmissions for the initial broadcast but one
      tsm_next_action.tx_delay = (GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter) ? ((random_rand() % (GLOSSY_MAX_JITTER_MULT+1))*GLOSSY_JITTER_STEP) : 0;
      TSM_TX_SLOT(&glossy_pt, glossy_next_action.buffer, glossy_next_action.is_rx?glossy_context.received_len:glossy_next_action.data_len);
      ++glossy_context.n_tx;
    }
  }

  tsm_next_action.progress_logic_slots = glossy_context.logic_deadline - tsm_prev_action.logic_slot_idx;
  tsm_next_action.progress_minislots = glossy_context.deadline - tsm_prev_action.minislot_idx;

  WARNIF(glossy_next_action.is_rx && glossy_next_action.max_len > 0 && glossy_context.rx_status == GLOSSY_RX_UNINITIALIZED);

  tsm_set_default_preambleto(glossy_context.original_preamble);

#if GLOSSY_LATENCY_LOG
  // If logging the latency of the flood is enabled, we log it at the end of the glossy flood
  if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
        PRINT("l %lu %lu", logging_context, elapsed);
  }
#endif

  PT_END(&glossy_pt);
}
