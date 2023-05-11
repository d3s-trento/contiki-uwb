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
 * \file      An glossy_tsm testing protocol
 *
 * \author    Enrico Soprana    <enrico.soprana@unitn.it>
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "contiki.h"

#include "contiki.h"
#include "lib/random.h"  // Contiki random
#include "sys/node-id.h"
#include "deployment.h"
#include "trex-tsm.h"
#include "trex.h"
#include "dw1000-conv.h"

#include "project-conf.h"
#include "src/pkts.h"
#include "src/local_context.h"

#include "deca_device_api.h"
#include "glossy_tsm.h"

#include "lcg-generator.h"
#include "bitmap_mapping.h"

#define LOG_PREFIX "n"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "print-def.h"
#include "clock.h"
#include "deca_regs.h"

#define PA tsm_prev_action
#define NA tsm_next_action

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#include "evb1000-timer-mapping.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

#pragma message STRDEF(ENHANCED_BOOTSTRAP)

static struct pt main_pt;    // protothread object

static pkt_t rcvd;
static pkt_t to_send;

struct local_context_t local_context;

static uint8_t buffer[127];     // buffer for TX and RX

#define MAX_FAILED_BOOTSTRAPS 3

#if !SNIFF_FS && SNIFF_FS_OFF_TIME != 0
#error "SNIFF_FS_OFF_TIME is set but also SNIFF_FS is 0"
#endif

extern bool statetime_tracking;

static uint32_t default_pto;

static char main_thread() {
  static uint16_t i;
  static bool is_originator;
  static uint16_t termination_counter;
  static uint16_t termination_cap;

  PT_BEGIN(&main_pt);

  local_context.epoch = 0;
  local_context.cumulative_failed_synchronizations = MAX_FAILED_BOOTSTRAPS;

  termination_cap = CONF_B + CONF_H;

  while (1) {
    tsm_set_default_preambleto(default_pto);

    local_context.hop_distance = UINT8_MAX;
    local_context.seen_event = false;

    if (node_id != SINK_ID) {
      termination_counter = 0;

      /* === Bootstrap phase === */
      NA.rx_guard_time = (1000 * UUS_TO_DWT_TIME_32); // start some time earlier than the sink
      while (1) {
        if (local_context.cumulative_failed_synchronizations < MAX_FAILED_BOOTSTRAPS) {
          TSM_RX_SLOT(&main_pt, buffer);
          termination_counter += 1;
        } else {
          TSM_SCAN(&main_pt, buffer);
        }

        if (PA.status == TREX_RX_SUCCESS && PA.payload_len == sizeof(pkt_t)) {
          memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(pkt_t));
          DBG("received synch(epoch %hu initiator %hu flag %xu hop %hu )", rcvd.epoch, rcvd.node_id, rcvd.flags, rcvd.hop_distance);

          if (local_context.epoch > rcvd.epoch) {
            ERR("Seen past epoch");

            // If we are going in past epochs try next time
            TSM_RESTART(&main_pt, PERIOD);
            continue;
          }

          // Synch to received packet
          NA.accept_sync = true;

          // Could have not received previous flood due to interference at that moment
          local_context.hop_distance = MIN(rcvd.hop_distance + 1, local_context.hop_distance);
          local_context.epoch = rcvd.epoch;

#if ENHANCED_BOOTSTRAP
          local_context.seen_event = (rcvd.flags & PKT_TYPE_EVENT) != 0;
#endif

          logging_context = rcvd.epoch;
          break;
        }

        if (termination_counter >= termination_cap) {
          break;
        }
      }

      if (PA.status != TREX_RX_SUCCESS && termination_counter >= termination_cap) {
        local_context.cumulative_failed_synchronizations = MIN(local_context.cumulative_failed_synchronizations + 1, MAX_FAILED_BOOTSTRAPS);

        WARN("Failed reception of the bootstrap, skipping epoch");

        // If we already passed the synchronization time, ignore until we reach the next epoch (and go back to beginning of the while)
        TSM_RESTART(&main_pt, PERIOD);
        continue;
      } else {
        // If we are here then we successfully synchronized, thus we should reset the failed synchronizations counter
        local_context.cumulative_failed_synchronizations = 0;
      }

      if (PA.logic_slot_idx >= CONF_H + CONF_B) {
        WARN("Skipped synch phase, restarting %hu", local_context.epoch);

        // If we already passed the synchronization time, ignore until we reach the next epoch (and go back to beginning of the while)
        TSM_RESTART(&main_pt, PERIOD);
        continue;
      }
    } else {
      local_context.hop_distance = 0;
      ++local_context.epoch;
    }

    //is_originator = (node_id == SINK_ID);
    //is_originator = (node_id == 119);
    is_originator = r_bitmap(local_context.epoch, N_ORIGINATORS) & flag_node(0x0, node_id);
    //is_originator = false;

    PRINT("epoch %hu hop %hu", local_context.epoch, local_context.hop_distance);

    /* === Bootstrap/Data collection phase === */
    while ((PA.logic_slot_idx + 1 - local_context.hop_distance < CONF_B) && (PA.logic_slot_idx + 1 < CONF_H + CONF_B)) { // Do all the re-transmissions for the initial broadcast but one
      to_send = (pkt_t){
        .hop_distance = local_context.hop_distance,
        .epoch        = local_context.epoch,
        .flags        = PKT_TYPE_SYNCH
      };

#if ENHANCED_BOOTSTRAP
      if (local_context.seen_event || is_originator) {
        to_send.flags |= PKT_TYPE_EVENT;
      }
#endif

      //NA.tx_delay = (random_rand() % (125+1))*0x2; // NOTE: Should redo using separate defines from fp

      memcpy(buffer + TSM_HDR_LEN, &to_send, sizeof(pkt_t));
      TSM_TX_SLOT(&main_pt, buffer, sizeof(pkt_t));
    }

    logging_context = local_context.epoch;

    STATETIME_MONITOR(dw1000_statetime_context_init());

    NA.progress_logic_slots = 15 + CONF_B + CONF_H - PA.logic_slot_idx;
    NA.progress_minislots = TSM_DEFAULT_MINISLOTS_GROUPING*(15 + CONF_B + CONF_H) + TSM_DEFAULT_MINISLOTS_GROUPING - 1 - PA.minislot_idx;

    tsm_set_default_preambleto(32*PRE_SYM_PRF64_TO_DWT_TIME_32);

    i = 0;

    while(i<GLOSSY_REPETITIONS) {
      if (local_context.epoch >= 10) {
        STATETIME_MONITOR(dw1000_statetime_start());
      }

      logging_context = local_context.epoch * GLOSSY_REPETITIONS + i;

      if (is_originator) {
        uint8_t tmp = random_rand();
        static uint8_t val[1];
        val[0] = tmp;

        memcpy(buffer + TSM_HDR_LEN, val, sizeof(val));

        glossy_next_action.is_rx = false;
        glossy_next_action.update_tref=false;
        glossy_next_action.max_len=10+N_TX; //SINK_RADIUS + N_TX;
        glossy_next_action.buffer=buffer;
        glossy_next_action.data_len=sizeof(val);
        glossy_next_action.N= N_TX;
        PT_SPAWN(&main_pt, &glossy_pt, glossy_trx());

        PRINT("TX_FS E %lu", logging_context);
      } else {

        glossy_next_action.is_rx = true;
        glossy_next_action.update_tref=false;
        glossy_next_action.max_len=10+N_TX; //SINK_RADIUS + N_TX;
        glossy_next_action.buffer=buffer;
        glossy_next_action.data_len=0;
        glossy_next_action.N=N_TX;
        PT_SPAWN(&main_pt, &glossy_pt, glossy_trx());
      }

      ++i;
      STATETIME_MONITOR(dw1000_statetime_stop());
    }

    logging_context = local_context.epoch;

    STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());
    //STATETIME_MONITOR(dw1000_statetime_stop_at(tsm_get_slot_end_4ns(PA.slot_idx + NA.progress_slots - 1)); printf("STATETIME "); dw1000_statetime_print());

    TSM_RESTART(&main_pt, PERIOD);
  }

  PT_END(&main_pt);
}

PROCESS(protocol_thread, "Peripheral protocol thread");
AUTOSTART_PROCESSES(&protocol_thread);
PROCESS_THREAD(protocol_thread, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  deployment_print_id_info();

  tsm_init();
  default_pto = tsm_get_default_preambleto();
  PRINT("DEFAULT_PREAMBLETO: %" PRIu32 "", default_pto);

  map_nodes();
  //print_nodes();

  static struct etimer et;
  etimer_set(&et, CLOCK_SECOND * 2);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)main_thread);

  PROCESS_END();
}

