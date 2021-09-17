/*
 * Copyright (c) 2020, University of Trento.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * \file      Time Slot Manager (TSM)
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 */

#define LOG_PREFIX "tsm"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#define TSM_LOG_SLOTS 1

#include "dw1000.h"
#include "dw1000-conv.h"
#include "dw1000-config.h"
#include <string.h>
#include "trex-tsm.h"
#include "trex.h"
#include "trex-driver.h"

/* Trex Slot Manager (TSM) is a module that simplifies scheduling slot-periodic
 * time structures (slot series) with TX or RX operations. Its features include
 * the following.
 *   - It hides time handling from the application.
 *   - It keeps track of the reference time of the slot series and allows scheduling
 *     a new series in the future.
 *   - It keeps track of the slot index and allows skipping slots, if needed.
 *   - It provides channel scanning and automatic synchronisation to a heard slot series.
 *   - It allows transmitting with a delay specified per-slot (which does not affect
 *     synchronisation).
 *   - It is tailored for protothreads (but can be used with regular callbacks, too).
 */

/* TSM slot structure is presented below. Slots are of fixed duration.
 * Each slot has the slot reference time, the earliest time when a packet SFD can be
 * received or transmitted (when the tx delay is set to 0).
 *
 * Packets can be delayed by tx delay, which is the difference between the actual
 * SFD time and the slot reference.
 *
 * RX slots have a fixed timeout, the time elapsed since the slot reference that forces
 * any reception to stop and allow time for packet processing at the end of the slot.
 *
 *                |<-------->| preamble
 *                | rx guard | duration
 * slot:      |___|__________|__________|_____________________________________________|
 *            ^                         ^                                             ^
 *            0                   slot reference                                slot duration
 *                                      |<--------------------->|<------------------->|
 *                                      |      slot timeout       time for processing
 *                                      |
 * packet:                            #############|========
 *                                      preamble   |  data
 *                                        |       SFD
 *                                        |<------>|
 *                                         tx delay
 *
 */

/* TSM schedules slots in series, one after another and calls the application callback
 * between slots. The callback can analyse the information about the previous slot and
 * affect the following slot using the following interface structures
 *   - tsm_prev_action
 *   - tsm_next_action
 *
 * When TSM is started with tsm_start(), it calls the application callback that must
 * fill in the structure defining the next action.
 *
 *      APP                              TSM
 *       | -------- tsm_start() --------> |
 *       |                                |
 *       | <-------- callback() --------- |
 *       | .....returns next action.....> |
 *       |                                | performs the requested action, when done, calls back
 *       | <-------- callback() --------- |
 *       | .....returns next action.....> |
 *       |                                |
 *      ...                              ...
 *
 *
 * The next action might be:
 *  - SCAN -- start listening right away until get a RX error or a correct
 *    reception. In case of correct reception, the TSM is synchronised to the
 *    slot series of the transmitter. Received packet (if any) is available to
 *    the application.
 *    Note that if this operation is invoked right after RESTART, the scanning
 *    will start at the very beginning of the next slot series.
 *
 *  - TX -- transmit in the next slot, according to the current slot series timing.
 *    Packet to transmit and the payload length must be provided.
 *
 *  - RX -- receive in the next slot, according to the current slot series timing.
 *    Received packet (if any) is available to the application.
 *
 *  - RESTART -- stop the current slot series and schedule a new one.
 *    The new series starts at the reference time of the previous series plus the
 *    specified interval. The callback will be called again to request the operation
 *    for the slot 0 of the new series.
 *
 *  - STOP -- stop everything.
 *
 *
 * The application may alter the following fields (set to default values) to modify
 * the action of the next slot:
 *  - tx delay -- shift the transmission (only positive shifts are allowed, default is 0)
 *  - slot progression -- used to skip slots if the application desires. By default is set
 *    to 1, meaning to progress to the next slot (no skip). Setting to 2 means skip 1 slot,
 *    and so on.
 *  - accept synchronisation -- if true, disregards the current synchronisation maintained
 *    by TSM and uses the one acquired from the previous slot. Only valid if there was a correct
 *    reception in the previous slot.
 *  - rx guard time -- adjust the guard time for the next slot (only RX slots)
 *
 *  The application must provide the buffer for TX/RX and SCAN operations, and the length
 *  of the payload for the TX operation.
 */


#ifdef TSM_CONF_DEFAULT_RXGUARD
#define TSM_DEFAULT_RXGUARD TSM_CONF_DEFAULT_RXGUARD
#else
#define TSM_DEFAULT_RXGUARD  (10*UUS_TO_DWT_TIME_32) // receivers guard time
#endif

#define TSM_CRC_OK (0xAE)

static struct {
  uint32_t         tref;             // reference time of the current slot series (reference time of slot 0)
  uint32_t         slot_duration;    // fixed slot duration
  uint32_t         slot_rx_timeout;  // for RX slots
  tsm_slot_cb      cb;               // higher-layer callback (called between slots)
  int16_t          slot_idx;         // current slot index
  enum tsm_action  slot_action;      // current slot action
  //uint32_t         slot_tref;        // current slot reference time (TODO)

  enum trex_status slot_status;         // Trex operation status for the current slot
  uint32_t         tentative_slot_tref; // slot reference of the received packet (if any)
  uint16_t         tentative_slot_idx;  // slot index of the received packet (if any)
  uint32_t         default_preambleto;
  uint16_t         default_preambleto_pacs;   // cache the default timeout in PACs units
} context;

/* Publicly accessible global interface structures used to exchange the
 * information about the previous and the next slot in the slot handler */
struct tsm_next_action tsm_next_action;
struct tsm_prev_action tsm_prev_action;

/* Default initialiser for the next action structure */
static struct tsm_next_action TSM_NEXT_ACTION_INITIALIZER = {
  .action = TSM_ACTION_NONE,
  //.slot_ref_time = 0, (TODO)
  .progress_slots = 1,
  .accept_sync = false,
  .tx_delay = 0,
  .restart_interval = 0,
  .rx_guard_time = TSM_DEFAULT_RXGUARD};

#define TSM_HDR_UPDATE(buf, hdr) memcpy((buf) + TSM_HDR_OFFS, hdr, TSM_HDR_LEN)
#define TSM_HDR_RETRIEVE(buf, hdr) memcpy((hdr), (buf) + TSM_HDR_OFFS, TSM_HDR_LEN)

/*-- Forward declarations for slot logging -----------------------------------*/

#if TSM_LOG_SLOTS
#define TSM_LOG_STATUS_RX_WITH_SYNCH (255) // make sure the value is compatible with  enum trex_status
_Static_assert (TSM_LOG_STATUS_RX_WITH_SYNCH > TREX_STATUS_ENUM_MAX, "Invalid constant");

struct tsm_log {
  uint8_t action;
  uint8_t status;
  int16_t idx_diff;
  int16_t slot_idx;
  uint8_t progress;
};

static inline void tsm_log_append(struct tsm_log *entry);
static inline void tsm_log_print();
static inline void tsm_log_init();
#endif

/*----------------------------------------------------------------------------*/

static inline
int tsm_tx(uint8_t *buffer, uint8_t payload_len) {
  DBGF();
  // calculate the time to TX
  uint32_t tx_sfd = context.tref
              + context.slot_idx*context.slot_duration
              + tsm_next_action.tx_delay;

  // update the header
  struct tsm_header hdr;
  hdr.slot_idx = context.slot_idx;
  hdr.tx_delay = tsm_next_action.tx_delay;
  hdr.crc      = TSM_CRC_OK;
  TSM_HDR_UPDATE(buffer, &hdr);

  return trexd_tx_at(buffer, payload_len + TSM_HDR_LEN, tx_sfd);
}

static inline
int tsm_rx(uint8_t *buffer) {
  DBGF();
  uint32_t expected_rx_sfd =
              context.tref 
              + context.slot_idx*context.slot_duration;
  
  uint32_t expected_rx_sfd_with_guard =
              expected_rx_sfd 
              - tsm_next_action.rx_guard_time;

  uint32_t deadline = expected_rx_sfd + context.slot_rx_timeout;

  return trexd_rx_slot(buffer, expected_rx_sfd_with_guard, deadline);
}

static inline
int tsm_scan(uint8_t *buffer) {
  DBGF();

  // at this point the slot and tref info are invalid
  return trexd_rx(buffer);
}

static inline
int tsm_scan_next_epoch(uint8_t *buffer) {
  DBGF();

  // at this point the slot and tref info are invalid
  // tref should point to a timestamp in the next epoch
  return trexd_rx_from(buffer, context.tref);
}


/* This function is the main interface with the higher layer.
 * It fills in the previous action interface structure,
 * calls the higher-layer callback, and analyses its response
 * recorded in the next action interface structure.
 */
static void tsm_slot_event() {
  bool recall;  // should we call the callback again?

  do {
    recall = 0; // by default we don't want to call it again

    // first, update the sync info if we were scanning
    // (this is processed in the slot after the successful scan
    //  takes place and allows the higher layer to have an up to
    //  date and trustable slot_idx)
    if (context.slot_action == TSM_ACTION_SCAN
          && context.slot_status == TREX_RX_SUCCESS) {
      context.slot_idx = context.tentative_slot_idx;
      context.tref = context.tentative_slot_tref - context.tentative_slot_idx*context.slot_duration;
    }

    // prepare the interface structures
    tsm_next_action = TSM_NEXT_ACTION_INITIALIZER;
    tsm_prev_action.action   = context.slot_action;
    tsm_prev_action.slot_idx = context.slot_idx;
    tsm_prev_action.status   = context.slot_status;
    tsm_prev_action.remote_slot_idx = context.tentative_slot_idx;

    DBG("Calling higher layer");
    context.cb();

    // If the higher layer explicitly requested sync
    // (we know this only after context.cb() is issued)
    if (tsm_next_action.accept_sync
          && context.slot_action == TSM_ACTION_RX
          && context.slot_status == TREX_RX_SUCCESS) {
      context.slot_idx = context.tentative_slot_idx;
      context.tref = context.tentative_slot_tref - context.tentative_slot_idx*context.slot_duration;
    }

    if (tsm_next_action.rx_guard_time == TSM_DEFAULT_RXGUARD) {
        // fall back to default preamble timeout for tx slots
        trexd_set_rx_slot_preambleto_pacs(context.default_preambleto_pacs);
    } else {
        // the gaurd time used has changed, ignore preamble timeout for
        // the next operation
        trexd_set_rx_slot_preambleto_pacs(0);
    }

    // the callback should have filled in the action request for
    // the next slot, now we look into it

#if TSM_LOG_SLOTS
    if (context.slot_status != TREX_NONE) {
      struct tsm_log entry;
      entry.status = context.slot_status;
      entry.action = context.slot_action;
      entry.progress = tsm_next_action.progress_slots;
      entry.slot_idx = context.slot_idx;
      if (context.slot_status == TREX_RX_SUCCESS) {
        entry.idx_diff = context.slot_idx - context.tentative_slot_idx;
      }
      else {
        entry.idx_diff = 0;
      }
      if (context.slot_status == TREX_RX_SUCCESS && (
            tsm_next_action.accept_sync || context.slot_action == TSM_ACTION_SCAN)) {
        entry.status = TSM_LOG_STATUS_RX_WITH_SYNCH; // mark that the sync was accepted
      }

      tsm_log_append(&entry);
    }
#endif

    // now, process the next requested action

    int ret = -1;
    context.slot_action = TSM_ACTION_NONE; /* in case anything goes wrong */

    switch(tsm_next_action.action) {
      case TSM_ACTION_TX:
        WARNIF(tsm_next_action.progress_slots == 0); // cannot TX in the same slot twice
        context.slot_idx  += tsm_next_action.progress_slots; // progress the slot idx
        ret = tsm_tx(tsm_next_action.buffer, tsm_next_action.payload_len);
        break;
      case TSM_ACTION_RX:
        WARNIF(tsm_next_action.progress_slots == 0); // continuing RX in the same slot is currently not implemented (TODO)
        context.slot_idx  += tsm_next_action.progress_slots; // progress the slot idx
        ret = tsm_rx(tsm_next_action.buffer);
        break;
      case TSM_ACTION_SCAN:
        if (context.slot_idx == -1) {
            // start listening in the next epoch
            context.slot_idx  = 0; // reset the slot idx, to an unitialized state
            ret = tsm_scan_next_epoch(tsm_next_action.buffer);
        }
        else {
            context.slot_idx  = 0; // reset the slot idx, to an unitialized state
            ret = tsm_scan(tsm_next_action.buffer);
        }
        break;
      case TSM_ACTION_RESTART:
        context.tref += tsm_next_action.restart_interval;
        context.slot_idx = -1;
        context.slot_status = TREX_NONE;
        recall = 1; // call the callback again
        trexd_stats_print();
        trexd_stats_reset();
#if TSM_LOG_SLOTS
        tsm_log_print();
#endif
        continue;   // restart the loop from the beginning
      case TSM_ACTION_STOP:
        return;

      default: ERR("Unexpected action requested %u", tsm_next_action.action);
        return;
    }

    if (ret >= 0) {
      // the action request was succesfull, so we must receive a callback
      // when it is done
      context.slot_action = tsm_next_action.action;
    }
    else {
      ERR("Failed to schedule a slot action %u", tsm_next_action.action);
      return;
      // TODO: try to recover instead, call the cb again reporting the error?
    }
  } while (recall);
}

/* Trex driver will call this function to notify of a completed action */
static void driver_slot_callback(const trexd_slot_t* slot) {
  DBGF();

  switch (context.slot_action) {
    case TSM_ACTION_TX:
      if (slot->status != TREX_TX_DONE) {
        ERR("Unexpected status %u for TX action", slot->status);
        return;
      }
      break;
    case TSM_ACTION_RX: case TSM_ACTION_SCAN:
      if (!TREX_IS_RX_STATUS(slot->status)) {
        ERR("Unexpected status %u for action %u", slot->status, context.slot_action);
        return;
      }
      break;
    default:
      ERR("Unexpected callback, action=%u", context.slot_action);
      return;
  }

  context.slot_status = slot->status;

  if (slot->status == TREX_RX_SUCCESS) {
    struct tsm_header hdr;
    if (slot->payload_len >= sizeof(hdr)) {
      TSM_HDR_RETRIEVE(slot->buffer, &hdr);

      if (hdr.crc != TSM_CRC_OK) {
          context.slot_status = TREX_RX_MALFORMED;
      }
      else {
        // memorise the sync information from the received packet in case
        // the higher layer decides to use it
        context.tentative_slot_tref = slot->trx_sfd_time_4ns - hdr.tx_delay;
        context.tentative_slot_idx = hdr.slot_idx;

        // This code is useful to debug mismatches
        /*
        int16_t idx_diff = context.slot_idx - context.tentative_slot_idx;
        size_t i = 0;
        if (context.slot_idx > 0 && idx_diff != 0) {
            printf("E %u, A %d R %d\n", logging_context, context.slot_idx, context.tentative_slot_idx);

            printf("E %u, B", logging_context);
            for (; i < slot->payload_len ; i ++) {
                printf(" %x", slot->buffer[i]);
            }
            printf("\n");

        }
        */
      }
    }
    else { // received an uncompliant packet
      context.slot_status = TREX_RX_MALFORMED;
    }
  }

  tsm_prev_action.payload_len = slot->payload_len - sizeof(struct tsm_header);
  tsm_prev_action.buffer = slot->buffer;
  tsm_prev_action.radio_status = slot->radio_status;
  tsm_slot_event();
}

/**
 * Convert preamble timeout from ~4ns units to the number of PACs.
 * A value of 0 disables the timeout.
 *
 * As expected by dwt_setpreambledetecttimeout(), returns the number of
 * PACs minus one, or zero to disable the timeout.
 */
static inline uint16_t tsm_preambleto_to_pacs(const dwt_config_t* config, const uint32_t timeout_4ns)
{
  if (timeout_4ns == 0) {
      return 0;
  }
  uint32_t symbol_duration_4ns;
  uint32_t pac_4ns;
  if (config->prf == DWT_PRF_16M) {
      symbol_duration_4ns = PRE_SYM_PRF16_TO_DWT_TIME_32;
  }
  else if (config->prf == DWT_PRF_64M) {
      symbol_duration_4ns = PRE_SYM_PRF64_TO_DWT_TIME_32;
  }
  else {
      ERR("Invalid PRF");
      return 0;
  }

  switch (config->rxPAC) {
      case DWT_PAC8  : pac_4ns = symbol_duration_4ns *  8;  break;
      case DWT_PAC16 : pac_4ns = symbol_duration_4ns * 16;  break;
      case DWT_PAC32 : pac_4ns = symbol_duration_4ns * 32;  break;
      case DWT_PAC64 : pac_4ns = symbol_duration_4ns * 64;  break;
      default: 
        ERR("Invalid PAC size."); 
        return 0;
  }
  // calculate the number of required PACs minus 1, since
  // dwt_setpreambledetecttimeout() automatically adds 1 to the given value
  uint16_t npacs = (timeout_4ns-1) / pac_4ns;

  if (npacs == 0) npacs = 1; // 0 has a special meaning, so we have to use 1

  return npacs;
}


void tsm_set_default_preambleto(const uint32_t preambleto)
{
  const dwt_config_t* radio_config = dw1000_get_current_cfg();
  context.default_preambleto = preambleto;
  context.default_preambleto_pacs = tsm_preambleto_to_pacs(radio_config, context.default_preambleto);
}

uint32_t tsm_get_default_preambleto()
{
  return context.default_preambleto;
}



/* Start a new slot structure roughly after one slot duration */
int tsm_start(uint32_t slot_duration, uint32_t rx_timeout, tsm_slot_cb callback) {
  if (callback == NULL)
    return -1;

  trexd_stats_reset();
  trexd_set_rx_slot_preambleto_pacs(context.default_preambleto_pacs);
  trexd_set_slot_callback(driver_slot_callback);
  context.cb = callback;
  context.tref = dwt_readsystimestamphi32() + slot_duration;
  context.slot_duration = slot_duration;
  context.slot_rx_timeout = rx_timeout;
  context.slot_idx = -1;
  context.slot_action = TSM_ACTION_NONE;

  // call back the higher layer before the first slot
  tsm_slot_event();
  return 0;
}


void tsm_init() {
  trexd_init();

  uint32_t symbol_duration_4ns = 0;
  uint32_t preamble_duration_4ns = 0;
  const dwt_config_t* radio_config = dw1000_get_current_cfg();
  if (radio_config->prf == DWT_PRF_16M) {
      symbol_duration_4ns = PRE_SYM_PRF16_TO_DWT_TIME_32;
  }
  else if (radio_config->prf == DWT_PRF_64M) {
      symbol_duration_4ns = PRE_SYM_PRF64_TO_DWT_TIME_32;
  }
  switch (radio_config->txPreambLength){
    case DWT_PLEN_4096: preamble_duration_4ns = symbol_duration_4ns * 4096; break;
    case DWT_PLEN_2048: preamble_duration_4ns = symbol_duration_4ns * 2048; break;
    case DWT_PLEN_1536: preamble_duration_4ns = symbol_duration_4ns * 1536; break;
    case DWT_PLEN_1024: preamble_duration_4ns = symbol_duration_4ns * 1024; break;
    case DWT_PLEN_512 : preamble_duration_4ns = symbol_duration_4ns *  512; break;
    case DWT_PLEN_256 : preamble_duration_4ns = symbol_duration_4ns *  256; break;
    case DWT_PLEN_128 : preamble_duration_4ns = symbol_duration_4ns *  128; break;
    case DWT_PLEN_64  : preamble_duration_4ns = symbol_duration_4ns *   64; break;
    default           : preamble_duration_4ns = symbol_duration_4ns *   64;
  }
  // listen for half the preamble length (+ the initial guard time)
  context.default_preambleto = preamble_duration_4ns / 2 + TSM_DEFAULT_RXGUARD;
  context.default_preambleto_pacs = tsm_preambleto_to_pacs(radio_config, context.default_preambleto);
}


/*-- Slot logging facility ---------------------------------------------------*/

#if TSM_LOG_SLOTS
#define TSM_LOGS_MAX 150
static struct tsm_log tsm_slot_logs[TSM_LOGS_MAX];
static size_t next_tsm_log = 0;

static inline void
tsm_log_init() {
    next_tsm_log = 0;
}

static inline void
tsm_log_append(struct tsm_log *entry) {
  if (next_tsm_log < TSM_LOGS_MAX) {
    tsm_slot_logs[next_tsm_log] = *entry;
    next_tsm_log++;
  }
}

static inline void
tsm_log_print() {

  struct tsm_log *s;

  printf("[" LOG_PREFIX " %lu]Slots: ", logging_context);
  for (int i=0; i<next_tsm_log; i++) {
    s = tsm_slot_logs + i;
    // print slot operation status
    if (s->action == TSM_ACTION_SCAN) {
      // mark scanning and the first received slot idx
      printf("_%d", -s->idx_diff);
      s->idx_diff = 0; // reset it to avoid printing the "m" modifier later
    }
    switch (s->status) {
      case TREX_TX_DONE:
        printf("T"); break; // Transmitted
      case TREX_RX_SUCCESS:
        printf("R"); break; // Received
      case TREX_RX_TIMEOUT:
        printf("L"); break; // Late
      case TREX_RX_ERROR:
        printf("E"); break; // Error
      case TREX_RX_MALFORMED:
        printf("B"); break; // Bad
      case TSM_LOG_STATUS_RX_WITH_SYNCH:
        printf("Y"); break; // received and resYnchronised
      default:
        printf("#");        // unknown (should not happen)
    }

    // print slot operation modifier

    if (s->idx_diff) {
      // difference between expected slot idx and the received one
      // in the previous slot (only print if non-zero)
      printf("m%+d", s->idx_diff);
    }
    if (s->progress != 1) {
      // slot progression value (only print if not 1)
      printf("p%d", s->progress);
    }
  }
  if (next_tsm_log == TSM_LOGS_MAX)
    printf("$");  // to mark potential overflow
  printf("\n");
  tsm_log_init();
}
#endif
