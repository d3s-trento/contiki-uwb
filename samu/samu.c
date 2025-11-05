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
 * \file      Synchronized Action Manager for UWB (SAMU) APIs
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 */

#define LOG_PREFIX "samu"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "dev/watchdog.h"

#include <inttypes.h>

#ifndef SAMU_LOG_SLOTS
#define SAMU_LOG_SLOTS 1
#endif

#include "dw1000.h"
#include "dw1000-conv.h"
#include "dw1000-util.h"
#include "dw1000-config.h"

#include <string.h>
#include "samu.h"
#include "samu-ral-status.h"
#include "samu-ral.h"

#include "samu-logs.h"

#include "contiki-samu.h"

#include "samu-radio-defaults.h"

#include "print-def.h"

/* SAMU is a module that simplifies scheduling slot-periodic
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

/* SAMU slot structure is presented below. Slots are of fixed duration.
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

/* SAMU schedules slots in series, one after another and calls the application callback
 * between slots. The callback can analyse the information about the previous slot and
 * affect the following slot using the following interface structures
 *   - samu_prev_action
 *   - samu_next_action
 *
 * When SAMU is started with samu_start(), it calls the application callback that must
 * fill in the structure defining the next action.
 *
 *      APP                              SAMU
 *       | -------- samu_start() -------->|
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
 *    reception. In case of correct reception, the SAMU is synchronised to the
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
 *    by SAMU and uses the one acquired from the previous slot. Only valid if there was a correct
 *    reception in the previous slot.
 *  - rx guard time -- adjust the guard time for the next slot (only RX slots)
 *
 *  The application must provide the buffer for TX/RX and SCAN operations, and the length
 *  of the payload for the TX operation.
 */

#ifndef SAMU_DEFAULT_SLIVERS_PER_ACTION
#define SAMU_DEFAULT_SLIVERS_PER_ACTION 1
#endif

#pragma message STRDEF(SAMU_DEFAULT_RXGUARD)
#pragma message STRDEF(SAMU_DEFAULT_SLIVERS_PER_ACTION)

#define SAMU_CRC_OK (0xAE)

struct samu_context_t context;

/* Publicly accessible global interface structures used to exchange the
 * information about the previous and the next slot in the slot handler */
struct samu_next_action samu_next_action;
struct samu_prev_action samu_prev_action;

/* Default initialiser for the next action structure */
static struct samu_next_action SAMU_NEXT_ACTION_INITIALIZER = {
  .action = SAMU_ACTION_NONE,
  //.slot_ref_time = 0, (TODO)
  .progress_logic_slots = 1,

  .progress_slivers = SAMU_DEFAULT_SLIVERS_PER_ACTION,
  .slivers_to_use = SAMU_DEFAULT_SLIVERS_PER_ACTION,

  .accept_sync = false,
  .tx_delay = 0,
  .restart_interval = 0,
  .restart_guard = 0,

  .rx_guard_time_before = SAMU_DEFAULT_RXGUARD,
  .rx_guard_time_after = 0,
  .rx_timeout = 0,

  .max_fs_flood_duration = 0,

  .drift_est_enable = true,
};

#define SAMU_HDR_UPDATE(buf, hdr) memcpy((buf) + SAMU_HDR_OFFS, hdr, SAMU_HDR_LEN)
#define SAMU_HDR_RETRIEVE(buf, hdr) memcpy((hdr), (buf) + SAMU_HDR_OFFS, SAMU_HDR_LEN)

/*-- Slot logging ------------------------------------------------------------*/

#if SAMU_LOG_SLOTS
#include "samu-logs.h"
#endif

/*- Slot functions -----------------------------------------------------------*/

static inline
int samu_tx(uint8_t *buffer, uint8_t payload_len) {
  DBGF();
  // calculate the time to TX
  uint32_t tx_sfd = context.tref
              + (context.sliver_idx + 1 - context.slivers_to_use)*context.slot_duration
              + SAMU_NEXT_A.tx_delay;

  // update the header
  struct samu_header hdr;
  hdr.sliver_idx = context.sliver_idx + 1 - context.slivers_to_use;
  hdr.tx_delay = SAMU_NEXT_A.tx_delay;
  hdr.crc      = SAMU_CRC_OK;
  SAMU_HDR_UPDATE(buffer, &hdr);

  return samu_rald_tx_at(buffer, payload_len + SAMU_HDR_LEN, tx_sfd);
}

static inline
int samu_rx(uint8_t *buffer) {
  DBGF();
  uint32_t expected_rx_sfd =
              context.tref 
              + (context.sliver_idx + 1 - context.slivers_to_use)*context.slot_duration;
  
  uint32_t expected_rx_sfd_with_guard =
              expected_rx_sfd 
              - SAMU_NEXT_A.rx_guard_time_before;

  uint32_t deadline = expected_rx_sfd + context.slot_rx_timeout;

  return samu_rald_rx_slot(buffer, expected_rx_sfd_with_guard, deadline);
}

static inline
int samu_rx_fp() {
  DBGF();
  uint32_t expected_rx_sfd =
              context.tref 
              + (context.sliver_idx + 1 - context.slivers_to_use)*context.slot_duration;

  uint32_t expected_rx_sfd_with_guard =
              expected_rx_sfd 
              - SAMU_NEXT_A.rx_guard_time_before;

  uint32_t deadline = expected_rx_sfd_with_guard + SAMU_NEXT_A.max_fs_flood_duration;

  return samu_rald_rx_slot_fp(expected_rx_sfd_with_guard, deadline);
}

static inline
int samu_tx_fp() {
  DBGF();

  // calculate the time to TX
  uint32_t tx_sfd = context.tref
              + (context.sliver_idx + 1 - context.slivers_to_use)*context.slot_duration
              + SAMU_NEXT_A.tx_delay;

  return samu_rald_tx_at_fp(tx_sfd);
}

static inline
int samu_scan(uint8_t *buffer) {
  DBGF();

  // at this point the slot and tref info are invalid
  return samu_rald_rx(buffer);
}

static inline
int samu_scan_next_epoch(uint8_t *buffer) {
  DBGF();

  // No need to take into account the slot index as this function should only only be 
  // called at the start of the epoch
  uint32_t expected_rx_sfd_with_guard = context.tref - SAMU_NEXT_A.rx_guard_time_before;

  // at this point the slot and tref info are invalid
  // tref should point to a timestamp in the next epoch
  return samu_rald_rx_from(buffer, expected_rx_sfd_with_guard);
}

struct radio_diagnostic_req samu_default_radio_diagnostic_req() {
	return samu_rald_default_radio_diagnostic_req();
}

struct radio_diagnostic samu_get_radio_diagnostic(struct radio_diagnostic_req req){
	return samu_rald_get_radio_diagnostic(req);
}

static void call_upper_layer() {
	  // Copy the previous progress and slivers to use so that they can be reported correctly later on (especially in the log)
	  SAMU_PREV_A.progress_slivers = SAMU_NEXT_A.progress_slivers;
	  SAMU_PREV_A.slivers_to_use = SAMU_NEXT_A.slivers_to_use;

	  // Reset samu_next_ation
      samu_next_action = SAMU_NEXT_ACTION_INITIALIZER;
	  SAMU_NEXT_A.rx_timeout = context.default_slot_rx_timeout;
      SAMU_NEXT_A.sniff = samu_rald_default_sniff_conf();

      // fill the remaining data of samu_prev_action
      SAMU_PREV_A.action   = context.slot_action;
      SAMU_PREV_A.logic_slot_idx = context.logic_slot_idx;
      SAMU_PREV_A.sliver_idx = context.sliver_idx;
      SAMU_PREV_A.status   = context.slot_status;
      SAMU_PREV_A.remote_logic_slot_idx = context.tentative_logic_slot_idx;
      SAMU_PREV_A.remote_sliver_idx = context.tentative_sliver_idx;

      DBG("Calling higher layer");
      context.cb();
}

/* This function is the main interface with the higher layer.
 * It fills in the previous action interface structure,
 * calls the higher-layer callback, and analyses its response
 * recorded in the next action interface structure.
 */
static void samu_slot_event() {
	static bool valid_rmarker;

	valid_rmarker = false;

	if (context.slot_action != SAMU_ACTION_RESTART) {
		// first, update the sync info if we were scanning
		// (this is processed in the slot after the successful scan
		//  takes place and allows the higher layer to have an up to
		//  date and trustable slot_idx)
		if (context.slot_action == SAMU_ACTION_SCAN
			  && context.slot_status == RX_SUCCESS) {
		  if (samu_rald_valid_rmarker(SAMU_PREV_A.radio_status)) {
		    context.sliver_idx = context.tentative_sliver_idx;
		    context.logic_slot_idx = context.tentative_logic_slot_idx;
		    context.tref = context.tentative_tref;

			context.first_tref = context.tref;

		    context.synchronised_epoch = true;
			valid_rmarker = true;

			// dw1000_trim(); // TODO : CHECK 1
		  } else {
			WARN("Requested sync on reception deemed with invalid RMARKER");
		  }
		}

		call_upper_layer();

		// If the higher layer explicitly requested sync
		// (we know this only after context.cb() is issued)
		if (SAMU_NEXT_A.accept_sync
			  && context.slot_action == SAMU_ACTION_RX
			  && context.slot_status == RX_SUCCESS) {
		  if (samu_rald_valid_rmarker(SAMU_PREV_A.radio_status)) {
			context.sliver_idx = context.tentative_sliver_idx;
			context.logic_slot_idx = context.tentative_logic_slot_idx;
			context.tref = context.tentative_tref;

			if (!context.synchronised_epoch) { // If first synchronization
				context.first_tref = context.tref;
			}

			context.synchronised_epoch = true;
			valid_rmarker = true;

			// dw1000_trim(); // TODO : CHECK 1
		  } else {
			WARN("Requested sync on reception deemed with invalid RMARKER");
		  }
		}
	}

	if ( ((SAMU_NEXT_A.action == SAMU_ACTION_RX) || (SAMU_NEXT_A.action == SAMU_ACTION_SCAN)) &&
		!samu_rald_sniff_conf_eq(context.sniff, SAMU_NEXT_A.sniff)
	) {
		samu_rald_sniff(SAMU_NEXT_A.sniff);
		context.sniff = SAMU_NEXT_A.sniff;

		SAMU_NEXT_A.rx_guard_time_before += samu_rald_sniff_conf_get_guard_offset(SAMU_NEXT_A.sniff);
	}

	context.slot_rx_timeout = SAMU_NEXT_A.rx_timeout;

	if ((SAMU_NEXT_A.action == SAMU_ACTION_RX) || (SAMU_NEXT_A.action == SAMU_ACTION_SCAN)) {
        // the gaurd time used has changed, ignore preamble timeout for the next operation
		// Make sure that rx_guard_time_after is not bigger than the maximum allowed time for reception 
		// NOTE(enrico.soprana): Could probably be way more conservative given the size of the preamble
		uint32_t fixed_rx_guard_time_after = context.slot_rx_timeout < SAMU_NEXT_A.rx_guard_time_after ? context.slot_rx_timeout : SAMU_NEXT_A.rx_guard_time_after;
		
		// default_preambleto is preamble_duration_4ns/2 + SAMU_DEFAULT_GUARD, so we can get the original value by subtracting SAMU_DEFAULT_GUARD
		uint32_t new_preambleto = (context.default_preambleto - SAMU_DEFAULT_RXGUARD) + SAMU_NEXT_A.rx_guard_time_before + fixed_rx_guard_time_after;

        samu_rald_set_rx_slot_preambleto(new_preambleto);
        //samu_rald_set_rx_slot_preambleto(0); // TODO: CHECK 1
    }

    // the callback should have filled in the action request for
    // the next slot, now we look into it

#if SAMU_LOG_SLOTS
    if (SAMU_PREV_A.status != STATUS_NONE) {
      samu_log_append(
			  SAMU_PREV_A.status,
			  SAMU_PREV_A.action,
			  SAMU_NEXT_A.accept_sync && valid_rmarker, // Note that only if the radio status shows a valid rmarker the variable valid_rmarker is set to true
			  SAMU_PREV_A.sliver_idx,
			  SAMU_PREV_A.slivers_to_use,
			  SAMU_PREV_A.progress_slivers,
			  (SAMU_PREV_A.status == RX_SUCCESS)? 
					SAMU_PREV_A.sliver_idx - context.tentative_sliver_idx 
					: 0);
    }
#endif

    // now, process the next requested action
    context.slivers_to_use = SAMU_NEXT_A.slivers_to_use;

    int ret = -1;

    // the action request was succesfull, so we must receive a callback when it is done
    context.slot_action = SAMU_NEXT_A.action;

    switch(SAMU_NEXT_A.action) {
      case SAMU_ACTION_TX:
        WARNIF(SAMU_NEXT_A.progress_logic_slots == 0); // cannot TX in the same slot twice
        context.logic_slot_idx  += SAMU_NEXT_A.progress_logic_slots; // progress the logic slot idx

        WARNIF(SAMU_NEXT_A.progress_slivers == 0); // cannot TX in the same slot twice
        context.sliver_idx += SAMU_NEXT_A.progress_slivers; // progress the sliver idx

        ret = samu_tx(SAMU_NEXT_A.buffer, SAMU_NEXT_A.payload_len);
        break;
      case SAMU_ACTION_RX:
        WARNIF(SAMU_NEXT_A.progress_logic_slots == 0); // continuing RX in the same slot is currently not implemented (TODO)
        context.logic_slot_idx  += SAMU_NEXT_A.progress_logic_slots; // progress the slot idx

        WARNIF(SAMU_NEXT_A.progress_slivers == 0); // cannot TX in the same slot twice
        context.sliver_idx += SAMU_NEXT_A.progress_slivers; // progress the sliver idx

        ret = samu_rx(SAMU_NEXT_A.buffer);
        break;
      case SAMU_ACTION_SCAN:
        if (context.logic_slot_idx == -1) {
            // start listening in the next epoch
            context.logic_slot_idx  = 0; // reset the slot idx, to an unitialized state
            context.sliver_idx = context.slivers_to_use - 1;

            ret = samu_scan_next_epoch(SAMU_NEXT_A.buffer);
        }
        else {
            context.logic_slot_idx  = 0; // reset the slot idx, to an unitialized state
            context.sliver_idx = context.slivers_to_use - 1;

            ret = samu_scan(SAMU_NEXT_A.buffer);
        }
        break;
      case SAMU_ACTION_RESTART:
		// We do not reset context.tref as it is used later in samu_pre_epoch_procedure
		context.current_restart_interval = SAMU_NEXT_A.restart_interval;
		context.current_restart_guard = SAMU_NEXT_A.restart_guard;
        context.logic_slot_idx = -1;
        context.sliver_idx = -1;
        context.slot_status = STATUS_NONE;
        context.slot_action = SAMU_ACTION_RESTART;
		context.sniff = samu_rald_default_sniff_conf();

		contiki_samu_epoch_restart();

#if FS_DEBUG
        fs_debug_log_print();
#endif
        samu_rald_stats_print();
        samu_rald_stats_reset();
#if SAMU_LOG_SLOTS
        samu_log_print();
#endif

        return;   // Restart is a special case, do not continue
      case SAMU_ACTION_EVENT:
        WARNIF(SAMU_NEXT_A.progress_logic_slots == 0); // cannot TX in the same slot twice
        context.logic_slot_idx  += SAMU_NEXT_A.progress_logic_slots; // progress the slot idx

        WARNIF(SAMU_NEXT_A.progress_slivers == 0); // cannot TX in the same slot twice
        context.sliver_idx += SAMU_NEXT_A.progress_slivers; // progress the sliver idx

        ret = samu_tx_fp();
        break;
      case SAMU_ACTION_EVENT_FP:
        WARNIF(SAMU_NEXT_A.progress_logic_slots == 0); // continuing RX in the same slot is currently not implemented (TODO)
        WARNIF(SAMU_NEXT_A.max_fs_flood_duration == 0);
        context.logic_slot_idx  += SAMU_NEXT_A.progress_logic_slots; // progress the slot idx

        WARNIF(SAMU_NEXT_A.progress_slivers == 0); // cannot TX in the same slot twice
        context.sliver_idx += SAMU_NEXT_A.progress_slivers; // progress the sliver idx

        ret = samu_rx_fp();
        break;
      case SAMU_ACTION_STOP:
        return;

      default: ERR("Unexpected action requested %u", SAMU_NEXT_A.action);
        return;
    }

    if (ret < 0) {
      context.slot_action = SAMU_ACTION_NONE;
      ERR("Failed to schedule a slot action %u at %" PRIu32 " (%" PRIu32 "-%" PRIu32 ")", SAMU_NEXT_A.action, context.logic_slot_idx, context.sliver_idx + 1 - context.slivers_to_use, context.sliver_idx);
      return;
      // TODO: try to recover instead, call the cb again reporting the error?
    }
}

/* RAL driver will call this function to notify of a completed action */
static void driver_slot_callback(const samu_rald_slot_t* slot) {
  static uint32_t old_status = 0;
  DBGF();

  if (slot == NULL) { // Special case for the restart, 
	context.slot_status = STATUS_NONE;
	context.slot_action = SAMU_ACTION_RESTART;

	samu_slot_event();
	return;
  }


  switch (context.slot_action) {
    case SAMU_ACTION_TX:
      if (slot->status != TX_DONE) {
        ERR("Unexpected status %u for TX action at %" PRIu32 " RS %" PRIu32 " %" PRIu32, slot->status, context.sliver_idx, old_status, slot->radio_status);
        return;
      }
      break;
    case SAMU_ACTION_RX: case SAMU_ACTION_SCAN: 
      if (!SAMU_IS_RX_STATUS(slot->status)) {
        ERR("Unexpected status %u for action %u", slot->status, context.slot_action);
        return;
      }
      break;
	case SAMU_ACTION_EVENT_FP: case SAMU_ACTION_EVENT:
	  break;
	case SAMU_ACTION_RESTART:
	  break;
  default:
      ERR("Unexpected callback, action=%u", context.slot_action);
      return;
  }

  old_status = slot->radio_status;
  context.slot_status = slot->status;

  if (slot->status == RX_SUCCESS) {
    struct samu_header hdr;
    if (slot->payload_len >= sizeof(hdr)) {
      SAMU_HDR_RETRIEVE(slot->buffer, &hdr);

      if (hdr.crc != SAMU_CRC_OK) {
          context.slot_status = RX_MALFORMED;
      }
      else {
        // memorise the sync information from the received packet in case
        // the higher layer decides to use it
        context.tentative_slot_tref = slot->trx_sfd_time_4ns - hdr.tx_delay;
        context.tentative_tref = context.tentative_slot_tref - (hdr.sliver_idx * context.slot_duration);
        context.tentative_sliver_idx = (hdr.sliver_idx + context.slivers_to_use - 1);
        context.tentative_logic_slot_idx = context.ms_to_ls_cb(hdr.sliver_idx);

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
      context.slot_status = RX_MALFORMED;
    }
  }

  SAMU_PREV_A.payload_len = slot->payload_len - sizeof(struct samu_header);
  SAMU_PREV_A.buffer = slot->buffer;
  SAMU_PREV_A.radio_status = slot->radio_status;
  samu_slot_event();
}

void samu_set_default_preambleto(const uint32_t preambleto)
{
  context.default_preambleto = preambleto;
}

uint32_t samu_get_default_preambleto()
{
  return context.default_preambleto;
}

uint32_t samu_get_default_rx_timeout() {
	return context.default_slot_rx_timeout;
}

uint32_t default_ms_to_ls(uint32_t sliver_idx) {
  /* For this default function we assume that the slots are homogenous and thus
   * it is enough to just get the number of slivers and divide for the number
   * of slivers that compose a normal slot
   */
  return sliver_idx / SAMU_DEFAULT_SLIVERS_PER_ACTION;
}

void null_driver_slot_callback() {
	driver_slot_callback(NULL);
}

/* Start a new slot structure using the sliver feature */
int samu_sliver_start(uint32_t slot_duration, uint32_t rx_timeout, samu_hl_cb callback, sliver_to_slot_cb sliver_to_slot_converter) {
  if (callback == NULL)
    return -1;

  samu_rald_stats_reset();
  samu_rald_set_rx_slot_preambleto(context.default_preambleto);
	//samu_rald_set_rx_slot_preambleto(0); // TODO: CHECK 1
  samu_rald_set_slot_callback(driver_slot_callback);
  context.cb = callback;
  context.ms_to_ls_cb = sliver_to_slot_converter;
  context.tref = samu_rald_now_time();
  context.first_tref = context.tref;
  context.slot_duration = slot_duration;
  context.default_slot_rx_timeout = rx_timeout;

  context.logic_slot_idx = -1;
  context.sliver_idx = -1;
  context.slot_status = STATUS_NONE;
  context.slot_action = SAMU_ACTION_RESTART;

  context.synchronised_epoch = false;

  call_upper_layer();
  contiki_samu_start();

  return 0;
}

/* Start a new slot structure roughly after one slot duration */
int samu_start(uint32_t slot_duration, uint32_t rx_timeout, samu_hl_cb callback) {
  /*
   * If this function is called instead of samu_sliver_start we assume the default
   * function (default_ms_to_ls) for the sliver to logic slot is enough.
   * In particular we assume that the slots are all homogenous.
   * Note that logic slots can simplify the tracking of protocol-level
   * "operations" in the schedule, but their use is entirely optional.
   */
  return samu_sliver_start(slot_duration, rx_timeout, callback, &default_ms_to_ls);
}

void samu_init() {
  samu_rald_init();
  samu_log_init();

  contiki_samu_init(&null_driver_slot_callback, &call_upper_layer, &context);

  samu_set_default_preambleto(samu_rald_get_base_preambleto() + SAMU_DEFAULT_RXGUARD);

  // listen for half the preamble length (+ the initial guard time)
  context.current_restart_interval = SAMU_PRE_EPOCH_PROCEDURE_GUARD_US + (EPOCH_INIT_DURATION + PRE_EPOCH_DURATION)*DWT_TICK_TO_NS_32/1000 + 1000000 ;
  context.current_restart_guard = 0;

  /* SAMUP-4, Moreover remove preceding part
  context.default_preambleto = samu_rald_get_base_preambleto() + SAMU_DEFAULT_RXGUARD;
  //*/
}

/*-- Utility functions -------------------------------------------------------*/

uint32_t samu_get_slot_start_4ns(uint16_t slot_idx) {
  return context.tref + (slot_idx * context.slot_duration);
}

uint32_t samu_get_slot_end_4ns(uint16_t slot_idx) {
  return samu_get_slot_start_4ns(slot_idx+1);
}

