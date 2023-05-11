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
 * \file      An Flick testing protocol
 *
 * \author    Enrico Soprana    <enrico.soprana@unitn.it>
 */

#include <stdio.h>
#include <string.h>
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

#include "lcg-generator.h"

#define LOG_PREFIX "n"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "print-def.h"
#include "clock.h"
#include "deca_regs.h"

#include "bitmap_mapping.h"

#define PA tsm_prev_action
#define NA tsm_next_action

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#include "evb1000-timer-mapping.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

static struct pt pt;    // protothread object

static pkt_t rcvd;
static pkt_t to_send;

struct local_context_t local_context;

static uint8_t buffer[127];     // buffer for TX and RX

#define MAX_FAILED_BOOTSTRAPS 3

#if !SNIFF_FS && SNIFF_FS_OFF_TIME != 0
#error "SNIFF_FS_OFF_TIME is set but also SNIFF_FS is 0"
#endif

extern bool statetime_tracking;

// void HardFault_Handler() {
//   printf("\n");
// }

static char main_thread() {
  static uint16_t i;
  static bool is_originator;
  static uint16_t termination_counter;
  static uint16_t termination_cap;

  PT_BEGIN(&pt);

  local_context.epoch = 0;
  local_context.cumulative_failed_synchronizations = MAX_FAILED_BOOTSTRAPS;

  termination_cap = CONF_B + CONF_H;

  while (1) {
    local_context.hop_distance = UINT8_MAX;

    if (node_id != SINK_ID) {
      termination_counter = 0;

      /* === Bootstrap phase === */
      NA.rx_guard_time = (1000 * UUS_TO_DWT_TIME_32); // start some time earlier than the sink
      while (1) {
        if (local_context.cumulative_failed_synchronizations < MAX_FAILED_BOOTSTRAPS) {
          TSM_RX_SLOT(&pt, buffer);
          termination_counter += 1;
        } else {
          TSM_SCAN(&pt, buffer);
        }

        if (PA.status == TREX_RX_SUCCESS && PA.payload_len == sizeof(pkt_t)) {
          // Synch to received packet
          NA.accept_sync = true;

          memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(pkt_t));
          DBG("received synch(epoch %hu initiator %hu flag %xu hop %hu )", rcvd.epoch, rcvd.node_id, rcvd.flags, rcvd.hop_distance);

          if (local_context.epoch > rcvd.epoch) {
            ERR("Seen past epoch");

            // If we are going in past epochs try next time
            TSM_RESTART(&pt, PERIOD);
            continue;
          }

          // Could have not received previous flood due to interference at that moment
          local_context.hop_distance = MIN(rcvd.hop_distance + 1, local_context.hop_distance);
          local_context.epoch = rcvd.epoch;

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
        TSM_RESTART(&pt, PERIOD);
        continue;
      } else {
        // If we are here then we successfully synchronized, thus we should reset the failed synchronizations counter
        local_context.cumulative_failed_synchronizations = 0;
      }

      if (PA.logic_slot_idx >= CONF_H + CONF_B) {
        WARN("Skipped synch phase, restarting %hu", local_context.epoch);

        // If we already passed the synchronization time, ignore until we reach the next epoch (and go back to beginning of the while)
        TSM_RESTART(&pt, PERIOD);
        continue;
      }
    } else {
      local_context.hop_distance = 0;
      ++local_context.epoch;
    }

    //is_originator = (node_id == SINK_ID);
    is_originator = (node_id == 119);
    //is_originator = r_bitmap(local_context.epoch, N_ORIGINATORS) & flag_node(0x0, node_id);
    //is_originator = false;

    PRINT("epoch %hu hop %hu", local_context.epoch, local_context.hop_distance);

    /* === Bootstrap/Data collection phase === */
    while ((PA.logic_slot_idx + 1 - local_context.hop_distance < CONF_B) && (PA.logic_slot_idx + 1 < CONF_H + CONF_B)) { // Do all the re-transmissions for the initial broadcast but one
      to_send = (pkt_t){
        .hop_distance = local_context.hop_distance,
        .epoch        = local_context.epoch,
        .flags        = PKT_TYPE_SYNCH,
        .node_id      = 0xff,
      };

      memcpy(buffer + TSM_HDR_LEN, &to_send, sizeof(pkt_t));
      TSM_TX_SLOT(&pt, buffer, sizeof(pkt_t));
    }

    logging_context = local_context.epoch;

    STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());

    NA.progress_logic_slots = CONF_B + CONF_H - PA.logic_slot_idx;
    NA.progress_minislots = TSM_DEFAULT_MINISLOTS_GROUPING*(CONF_B + CONF_H) + TSM_DEFAULT_MINISLOTS_GROUPING - 1 - PA.minislot_idx;

    i=0;

    while (i<FS_PER_EPOCH) {
      STATETIME_MONITOR(dw1000_statetime_start());
      logging_context = ((i & 0xff) << 17) | local_context.epoch;

      statetime_tracking = false;
      NA.max_fs_flood_duration = MAX_FS_LATENCY;
      if (is_originator) {
        PRINT("TX_FS E %" PRIu32 , logging_context);
        TSM_TX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
      } else {
        NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
        TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
      }
      statetime_tracking = false;

      STATETIME_MONITOR(dw1000_statetime_stop());

      NA.rx_guard_time = 0;
      TSM_RX_SLOT(&pt, buffer);

      NA.rx_guard_time = 0;
      TSM_RX_SLOT(&pt, buffer);

      i++;
    }

    logging_context = local_context.epoch;

    STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());

    TSM_RESTART(&pt, PERIOD);
  }

  PT_END(&pt);
}

PROCESS(protocol_thread, "Peripheral protocol thread");
AUTOSTART_PROCESSES(&protocol_thread);
PROCESS_THREAD(protocol_thread, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  deployment_print_id_info();

  tsm_init();

  map_nodes();
  //print_nodes();

  //tsm_set_default_preambleto(0); // disable preamble timeout
  PRINT("DEFAULT_PREAMBLETO: %lu", tsm_get_default_preambleto());

  printf("FS conf:\n"
         " max_latency: %lu\n"
         " sniff_fs: %hu\n"
         " sniff_fs_off_time: %hu\n",
         MAX_FS_LATENCY,
         SNIFF_FS,
         SNIFF_FS_OFF_TIME);


  static struct etimer et;
  etimer_set(&et, CLOCK_SECOND * 2);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)main_thread);

  PROCESS_END();
}

