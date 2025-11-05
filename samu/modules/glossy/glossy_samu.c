/*
 * Copyright (c) 2025, University of Trento, Italy
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * \file SAMU Glossy API
 *
 * Authors:
 *   Enrico Soprana <enrico.soprana@unitn.it>
 */
#include "glossy_samu.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "samu-ral-status.h"
#include "samu-ral.h"
#include "samu.h"

#include "lib/random.h"

#include "dw1000-conv.h"
#include "print-def.h"

#define LOG_PREFIX "gt"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#pragma message STRDEF(GLOSSY_LATENCY_LOG)
#pragma message STRDEF(GLOSSY_MAX_JITTER_MULT)
#pragma message STRDEF(GLOSSY_JITTER_STEP)

#pragma message STRDEF(SAMU_DEFAULT_RXGUARD)
#pragma message STRDEF(SAMU_DEFAULT_SLIVERS_PER_ACTION)

struct glossy_context_t glossy_context;
struct pt glossy_pt;
struct glossy_next_action_t glossy_next_action;

static void glossy_next_action_reset() {
  glossy_next_action = (struct glossy_next_action_t){
      .is_rx = false,
      .update_tref = false,
      .max_len = 0,
      .N = 0,
      .buffer = NULL,
      .data_len = 0,
      .jitter = false,
      .slivers_per_action = SAMU_DEFAULT_SLIVERS_PER_ACTION,
      .rx_guard_time_before = SAMU_DEFAULT_RXGUARD,
      .rx_guard_time_after = 0,
      .rx_timeout = samu_get_default_rx_timeout()};
}

void glossy_init() { glossy_next_action_reset(); }

uint32_t glossy_samu_sliver_last_rx = 0;

void reset_context() {
  glossy_context.rx_status = GLOSSY_RX_UNINITIALIZED;
  glossy_context.received_len = 0;
  glossy_samu_sliver_last_rx = 0;
}

char glossy_trx();

#ifndef GLOSSY_LATENCY_LOG
#define GLOSSY_LATENCY_LOG 0
#endif

#if GLOSSY_LATENCY_LOG
#include "deca_device_api.h"
#include "dw1000-statetime.h"

static uint32_t elapsed;
#endif

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
  WARNIF(glossy_next_action.slivers_per_action == 0);
  WARNIF(glossy_next_action.rx_timeout == 0);

  if (!glossy_next_action.is_rx) {
    WARNIF(glossy_next_action.data_len == 0);
  }

  glossy_context.original_preamble = samu_get_default_preambleto();

  // Change the preambleto to take into account the additional jitter (enabled
  // when GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter)
  samu_set_default_preambleto(
      glossy_context.original_preamble +
      ((GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter)
           ? (GLOSSY_JITTER_STEP * GLOSSY_MAX_JITTER_MULT)
           : 0)); // add jitter to preambleto if present

  reset_context();

#if GLOSSY_LATENCY_LOG
  elapsed = -1;
#endif

  // NOTE: We do not modify progress_slots as this is part of the interface to
  // interact with glossy_samu

  glossy_context.n_tx = 0;
  glossy_context.logic_deadline = SAMU_PREV_A.logic_slot_idx +
                                  SAMU_NEXT_A.progress_logic_slots +
                                  glossy_next_action.max_len;
  glossy_context.deadline =
      SAMU_PREV_A.sliver_idx + SAMU_NEXT_A.progress_slivers -
      SAMU_DEFAULT_SLIVERS_PER_ACTION +
      (glossy_next_action.max_len * glossy_next_action.slivers_per_action);

  if (glossy_next_action.max_len == 0) {
    WARN("mi %" PRIu32 " pms %" PRIu32 " dl %" PRIu32, SAMU_PREV_A.sliver_idx,
         SAMU_NEXT_A.progress_slivers, glossy_context.deadline);
  }

  if (glossy_next_action.is_rx) {
    // Try to receive something until the next slot is after at or past deadline
    while (SAMU_PREV_A.sliver_idx + SAMU_NEXT_A.progress_slivers +
               glossy_next_action.slivers_per_action -
               SAMU_DEFAULT_SLIVERS_PER_ACTION <=
           glossy_context.deadline) {
      SAMU_SET_SLIVERS_PER_ACTION(glossy_next_action.slivers_per_action);

      SAMU_NEXT_A.rx_guard_time_before =
          glossy_next_action.rx_guard_time_before;
      SAMU_NEXT_A.rx_guard_time_after = glossy_next_action.rx_guard_time_after;
      SAMU_NEXT_A.rx_timeout = glossy_next_action.rx_timeout;
      SAMU_RX(&glossy_pt, glossy_next_action.buffer);

#if GLOSSY_LATENCY_LOG
      if (glossy_context.first_rx) {
        /*
         * If this is the first reception slot, save the time at which the
         * reception started, this will be used later to obtain the latency of
         * the glossy flood
         */
        glossy_context.start_4ns = dw1000_statetime_get_schedule_32hi();
        glossy_context.first_rx = false;
      }
#endif

      if (SAMU_PREV_A.status == RX_SUCCESS) {
        glossy_context.rx_status = GLOSSY_RX_SUCCESS;

        glossy_context.received_len = SAMU_PREV_A.payload_len;

        if (SAMU_PREV_A.payload_len == 0) {
          WARN("Received null packet at %" PRIu32, SAMU_PREV_A.logic_slot_idx);
        }

        if (glossy_next_action.update_tref) {
          SAMU_NEXT_A.accept_sync = 1;
        }

#if GLOSSY_LATENCY_LOG
        /*
         * If the options GLOSSY_LATENCY_LOG is enabled, obtain the difference
         * in time between the start of the first glossy flood and the RMARKER
         * of the received packet. Afterwards convert it to ns and add to it the
         * rest of the packet (estimated from the payload length and radio
         * configuration)
         */
        elapsed = dwt_readrxtimestamphi32();
        elapsed -= glossy_context.start_4ns;

        elapsed = ((uint32_t)(elapsed * DWT_TICK_TO_NS_32));

        elapsed += estimate_payload_time_ns(
            glossy_context.received_len + SAMU_HDR_LEN +
            2); // The amount we received + the SAMU_RALD_FRAME_OVERHEAD (due 2
                // byte CRC)
#endif
        glossy_samu_sliver_last_rx = SAMU_PREV_A.sliver_idx;

        break;
      } else if (SAMU_PREV_A.status == RX_ERROR) {
        if (glossy_context.rx_status != GLOSSY_RX_SUCCESS) {
          glossy_context.rx_status = GLOSSY_RX_ERROR;
        }
      } else if (SAMU_PREV_A.status == RX_TIMEOUT) {
        if (glossy_context.rx_status == GLOSSY_RX_UNINITIALIZED) {
          glossy_context.rx_status = GLOSSY_RX_TIMEOUT;
        }
      }
    }
  }

  /*
   * If this is a transmission(!is_rx) or we received with success (and thus we
   * have to re-propagate the message) re-transmit at max N times within the
   * allotted time for the glossy flood
   */
  if (!glossy_next_action.is_rx ||
      (glossy_context.rx_status == GLOSSY_RX_SUCCESS)) {
    if (glossy_next_action.is_rx && glossy_context.received_len == 0) {
      WARN("is_rx && data_len == 0 at %" PRIu32 " (%" PRIu32
           ")with rx_status %hu",
           SAMU_PREV_A.logic_slot_idx, SAMU_PREV_A.sliver_idx,
           glossy_context.rx_status);
    }

    while ((glossy_context.n_tx < glossy_next_action.N) &&
           (SAMU_PREV_A.sliver_idx + SAMU_NEXT_A.progress_slivers +
                glossy_next_action.slivers_per_action -
                SAMU_DEFAULT_SLIVERS_PER_ACTION <=
            glossy_context.deadline)) { // Do all the re-transmissions for the
                                        // initial broadcast but one
      SAMU_NEXT_A.tx_delay =
          (GLOSSY_MAX_JITTER_MULT > 0 && glossy_next_action.jitter)
              ? ((random_rand() % (GLOSSY_MAX_JITTER_MULT + 1)) *
                 GLOSSY_JITTER_STEP)
              : 0;
      SAMU_SET_SLIVERS_PER_ACTION(glossy_next_action.slivers_per_action);
      SAMU_TX(&glossy_pt, glossy_next_action.buffer,
              glossy_next_action.is_rx ? glossy_context.received_len
                                       : glossy_next_action.data_len);
      ++glossy_context.n_tx;
    }
  }

  SAMU_NEXT_A.progress_logic_slots =
      glossy_context.logic_deadline - SAMU_PREV_A.logic_slot_idx;
  SAMU_NEXT_A.progress_slivers = glossy_context.deadline +
                                 SAMU_DEFAULT_SLIVERS_PER_ACTION -
                                 SAMU_PREV_A.sliver_idx;

  WARNIF(glossy_next_action.is_rx && glossy_next_action.max_len > 0 &&
         glossy_context.rx_status == GLOSSY_RX_UNINITIALIZED);

  samu_set_default_preambleto(glossy_context.original_preamble);

#if GLOSSY_LATENCY_LOG
  // If logging the latency of the flood is enabled, we log it at the end of the
  // glossy flood
  if (glossy_context.rx_status == GLOSSY_RX_SUCCESS) {
    PRINT("l %lu %lu", logging_context, elapsed);
  }
#endif

  glossy_next_action_reset();

  PT_END(&glossy_pt);
}
