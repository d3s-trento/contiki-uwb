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
 * \file      An glossy_samu testing protocol
 *
 * \author    Enrico Soprana    <enrico.soprana@unitn.it>
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "contiki.h"

#include <math.h>

#include "lib/random.h"  // Contiki random
#include "sys/node-id.h"
#include "deployment.h"
#include "samu-ral.h" // required for SAMU_RALD_FRAME_OVERHEAD
#include "samu.h"
#include "samu-ral-status.h"
#include "dw1000-conv.h"

#include "project-conf.h"
#include "src/pkts.h"
#include "src/local_context.h"

#include "deca_device_api.h"
#include "dev/watchdog.h"

#include "glossy_samu.h"
#include "bitmap_mapping.h"
#include "lcg-generator.h"

#define LOG_PREFIX "n"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "print-def.h"
#include "clock.h"
#include "deca_regs.h"


#define PA SAMU_PREV_A
#define NA SAMU_NEXT_A

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#include "evb1000-timer-mapping.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

static struct pt main_pt;    // protothread object

static pkt_t rcvd;
static pkt_t to_send;

struct local_context_t local_context;

static uint32_t max_glossy;

static uint8_t buffer[127];     // buffer for TX and RX

#define MAX_FAILED_BOOTSTRAPS 2
#define NUM_RECOVERY_BOOTSTRAPS 3

#define NODE_RESTART()         do {if (local_context.epoch < 10) SAMU_NEXT_A.drift_est_enable = false; SAMU_RESTART(&main_pt, PERIOD_US); } while(0)
#define SINK_RESTART()         do {if (local_context.epoch < 10) SAMU_NEXT_A.drift_est_enable = false; SAMU_RESTART(&main_pt, PERIOD_US); } while(0)

#define NODE_GUARDED_RESTART() do {if (local_context.epoch < 10) SAMU_NEXT_A.drift_est_enable = false; SAMU_RESTART_GUARDED(&main_pt, PERIOD_US, (((uint64_t)PERIOD_US)*20*MAX_FAILED_BOOTSTRAPS + 999999)/1000000); } while(0)
#define SINK_GUARDED_RESTART() do { WARN("Sink is doing guarded restart?"); if (local_context.epoch < 10) SAMU_NEXT_A.drift_est_enable = false; SAMU_RESTART(&main_pt, PERIOD_US); } while(0)

static inline uint32_t get_rx_pwr() {
  uint16_t rxPreamCount = (dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXPACC_MASK) >> RX_FINFO_RXPACC_SHIFT;
  uint16_t pacNonsat = dwt_read16bitoffsetreg(DRX_CONF_ID, 0x2C);

  uint16_t maxGrowthCIR = dwt_read16bitoffsetreg(RX_FQUAL_ID, 0x06);

  /* As per "4.7.2 Estimating the receive signal power" pg. 46, RXPACC before being used has to 
   * be adjusted. The adjustment is at Table 18 of the manual and, assuming the standard 8 symbols SFD, is -5.
   * This adjustment has to be applied only when RXPACC and RXPACC_NOSAT are the same
   */
  uint32_t adjusted_rxpacc = rxPreamCount - ((pacNonsat == rxPreamCount)?5:0);

  uint64_t maxGrowthCIR_64 = maxGrowthCIR;
  maxGrowthCIR_64 <<= 48;

  uint64_t quad_adjusted_rxpacc = adjusted_rxpacc;
  quad_adjusted_rxpacc *= quad_adjusted_rxpacc;

  // Check if the power is high enough to accept the node as the parent
  return maxGrowthCIR_64 / quad_adjusted_rxpacc;
}

double get_rx_pwr2(){
  uint16_t rxPreamCount = (dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXPACC_MASK) >> RX_FINFO_RXPACC_SHIFT;
  uint16_t pacNonsat = dwt_read16bitoffsetreg(DRX_CONF_ID, 0x2C);

  uint16_t maxGrowthCIR = dwt_read16bitoffsetreg(RX_FQUAL_ID, 0x06);

  /* Compute corrected preamble counter (used for CIR power adjustment) */
  int16_t corrected_pac;
  if(rxPreamCount == pacNonsat) {
    // NOTE this is only valid for standard SFDs!
    int16_t sfd_correction =  5;
    corrected_pac = rxPreamCount - sfd_correction;
  }
  else {
    corrected_pac = rxPreamCount;
  }

  double pac_correction = corrected_pac;
  pac_correction *= pac_correction;

  /* Compute the CIR power level, corrected by PAC value */
  double cir_pwr_norm = (double)maxGrowthCIR * (double)((uint32_t)1 << 17) / pac_correction;
  double prf_correction = 121.74;

  // /* Compute RX power and First-Path RX power */
  // d->fp_raw = (double)(POW2(rxdiag->firstPathAmp1) + POW2(rxdiag->firstPathAmp2) + POW2(rxdiag->firstPathAmp3)) / d->pac_correction;
  // if(d->cir_pwr_norm != 0 && d->fp_raw != 0) {
  //   double prf_correction = (config->prf == DWT_PRF_64M) ? 121.74 : 113.77;
  //   d->rx_pwr = 10 * log10(d->cir_pwr_norm) - prf_correction;
  //   d->fp_pwr = 10 * log10(d->fp_raw) - prf_correction;
  // }
  // else {
  //   d->rx_pwr = 0;
  //   d->fp_pwr = 0;
  //   return false;
  // }

  return 10 * log10(cir_pwr_norm) - prf_correction;

}

static char main_thread() {
  static uint16_t termination_counter;
  static uint16_t termination_cap;

  static uint8_t scans_to_perform;

  static uint8_t n_tx;
  static bool scanned;
  static int err = 0;

  PT_BEGIN(&main_pt);

  glossy_init();

  local_context.epoch = 0;
  local_context.cumulative_failed_synchronizations = (node_id != SINK_ID)? MAX_FAILED_BOOTSTRAPS : 0;

  scans_to_perform = (node_id != SINK_ID)? 150: 0;

  termination_cap = CONF_B + CONF_H;

  while (1) {

    local_context.hop_distance = UINT8_MAX;
    local_context.seen_event = false;

	STATETIME_MONITOR(dw1000_statetime_context_init());

	STATETIME_MONITOR(if (local_context.epoch < 20) {
		dw1000_statetime_stop();
	} else {
		dw1000_statetime_start();
	});

	scanned = false;

    if (node_id != SINK_ID) {
      termination_counter = 0;

      /* === Bootstrap phase === */
      while (1) {
		NA.rx_guard_time_before = 16 * UUS_TO_DWT_TIME_32; // start some time earlier than the sink
		NA.rx_guard_time_after =  16 * UUS_TO_DWT_TIME_32;
		NA.rx_timeout = (43 + 16) * UUS_TO_DWT_TIME_32;

		// NA.rx_guard_time_before = 12 * UUS_TO_DWT_TIME_32; // start some time earlier than the sink
		// NA.rx_guard_time_after =   4 * UUS_TO_DWT_TIME_32;
		// NA.rx_timeout = (43 +  4) * UUS_TO_DWT_TIME_32;

        if ((local_context.cumulative_failed_synchronizations < MAX_FAILED_BOOTSTRAPS) && (scans_to_perform == 0)) {
		  NA.sniff = (struct sniff_conf_t){.enable = false, .rx_pacs = 1, .sleep_us = 16};
          SAMU_RX(&main_pt, buffer);
          termination_counter += 1;
        } else {
		  NA.sniff = (struct sniff_conf_t){.enable = true, .rx_pacs = 1, .sleep_us = 16};
          SAMU_SCAN(&main_pt, buffer);
		  scanned = true;
        }

		if (PA.status == RX_SUCCESS && PA.payload_len == sizeof(pkt_t)) {
		  memcpy(&rcvd, buffer + SAMU_HDR_LEN, sizeof(pkt_t));
		  DBG("received synch(epoch %hu initiator %hu flag %xu hop %hu )", rcvd.epoch, rcvd.node_id, rcvd.flags, rcvd.hop_distance);

		  if (local_context.epoch > rcvd.epoch) {
			break;
		  }

		  // Synch to received packet
		  NA.accept_sync = true;

		  // Could have not received previous flood due to interference at that moment
		  local_context.hop_distance = MIN(rcvd.hop_distance + 1, local_context.hop_distance);
		  local_context.epoch = rcvd.epoch;

		  scans_to_perform = (scans_to_perform == 0)? 0: (scans_to_perform - 1);

		  logging_context = rcvd.epoch;

		  err = 0;

		  break;
		}

        if (termination_counter >= termination_cap) {
          break;
        }
      }

	  if (local_context.epoch > rcvd.epoch) {
		ERR("Seen past epoch");

		// If we are going in past epochs try next time
		if (node_id == SINK_ID) {
			SINK_RESTART();
		} else {
			NODE_RESTART();
		}
	  } else if (PA.status != RX_SUCCESS && termination_counter >= termination_cap) {
        local_context.cumulative_failed_synchronizations = MIN(local_context.cumulative_failed_synchronizations + 1, MAX_FAILED_BOOTSTRAPS);

        WARN("Failed reception of the bootstrap, skipping epoch");
        WARN("Failed reception of the bootstrap with %lu", PA.radio_status);

        // If we already passed the synchronization time, ignore until we reach the next epoch (and go back to beginning of the while)
        if ((local_context.cumulative_failed_synchronizations < MAX_FAILED_BOOTSTRAPS) && (scans_to_perform == 0)) {
			if (node_id == SINK_ID) {
				SINK_RESTART();
			} else {
				NODE_RESTART();
			}
		} else {
			scans_to_perform = NUM_RECOVERY_BOOTSTRAPS;

			if (node_id == SINK_ID) {
				SINK_GUARDED_RESTART();
			} else {
				NODE_GUARDED_RESTART();
			}
		}

        continue;
      } else {
        // If we are here then we successfully synchronized, thus we should reset the failed synchronizations counter
        local_context.cumulative_failed_synchronizations = 0;
      }

      if (PA.logic_slot_idx >= CONF_H + CONF_B) {
        WARN("Skipped synch phase, restarting %hu", local_context.epoch);

        // If we already passed the synchronization time, ignore until we reach the next epoch (and go back to beginning of the while)
		if (node_id == SINK_ID) {
			SINK_RESTART();
		} else {
			NODE_RESTART();
		}
        continue;
      }
    } else {
      local_context.hop_distance = 0;
      ++local_context.epoch;
    }

	if (local_context.epoch > 21) {
		STATETIME_MONITOR(dw1000_statetime_pre_epoch_take_into_account());
	}
	STATETIME_MONITOR(dw1000_statetime_stop());

    PRINT("epoch %" PRIu16 " hop %" PRIu8 " scan %i", local_context.epoch, local_context.hop_distance, scanned);

    /* === Bootstrap/Data collection phase === */
	n_tx = 0;
    while ((n_tx < CONF_B) && (PA.logic_slot_idx + 1 < CONF_H + CONF_B)) { // Do all the re-transmissions for the initial broadcast but one
																		   // && !(PA.radio_status & (SYS_STATUS_RFPLL_LL | SYS_STATUS_CLKPLL_LL))
																		   //  && !(PA.radio_status & (SYS_STATUS_CLKPLL_LL))
      to_send = (pkt_t){
        .hop_distance = local_context.hop_distance,
        .epoch        = local_context.epoch,
        .flags        = PKT_TYPE_SYNCH
      };

      //NA.tx_delay = (random_rand() % (125+1))*0x2; // NOTE: Should redo using separate defines from fp

      memcpy(buffer + SAMU_HDR_LEN, &to_send, sizeof(pkt_t));
      SAMU_TX(&main_pt, buffer, sizeof(pkt_t));

	  ++n_tx;
    }

	NA.progress_slivers = 100 - PA.sliver_idx;

    logging_context = local_context.epoch;

	static uint16_t i;

	static uint32_t num_glossy;

	num_glossy = r_num(local_context.epoch) % max_glossy;

	i = 0;
	while(i < num_glossy) {
	  logging_context = local_context.epoch * GLOSSY_REPETITIONS + i;

	  if (node_id == SINK_ID) {
		uint8_t tmp = random_rand();
		static uint8_t val[1];
		val[0] = tmp;

		memcpy(buffer + SAMU_HDR_LEN, val, sizeof(val));
        SAMU_GLOSSY(&main_pt,
				    GLOSSY_INIT,
					((struct glossy_cfg_t){.W = 10+N_TX, .N=N_TX, .L=2}),+
					buffer,
					sizeof(val));
		//GLOSSY_TX(&main_pt, 10+N_TX, N_TX, buffer, sizeof(val));
	  } else {
		glossy_next_action.update_tref = true;
        SAMU_GLOSSY(&main_pt,
				    GLOSSY_FWD,
					((struct glossy_cfg_t){.W = 10+N_TX, .N=N_TX, .L=2}),
					buffer,
					0);
		//GLOSSY_RX(&main_pt, 10+N_TX, N_TX, buffer);
	  }

	  ++i;
	}

    logging_context = local_context.epoch;

    STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());

	if ((local_context.cumulative_failed_synchronizations < MAX_FAILED_BOOTSTRAPS) && (scans_to_perform == 0)) {
		if (node_id == SINK_ID) {
			SINK_RESTART();
		} else {
			NODE_RESTART();
		}
	} else {
		if (node_id == SINK_ID) {
			SINK_GUARDED_RESTART();
		} else {
			NODE_GUARDED_RESTART();
		}
	}
  }

  PT_END(&main_pt);
}

PROCESS(protocol_thread, "Peripheral protocol thread");
AUTOSTART_PROCESSES(&protocol_thread);
PROCESS_THREAD(protocol_thread, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  deployment_print_id_info();

  map_nodes();

  samu_init();

#if (PERIOD_US < (35 * 1000000))
#define MAX_DIV_ACTIVE_EPOCH 3
#else
#define MAX_DIV_ACTIVE_EPOCH 10
#endif

#pragma message STRDEF(MAX_DIV_ACTIVE_EPOCH)

  max_glossy = (((PERIOD_US * 1000. / DWT_TICK_TO_NS_32) / SLOT_DURATION - 100) / MAX_DIV_ACTIVE_EPOCH) / (10 + N_TX);

  PRINT("max_glossy %"PRIu32, max_glossy);

  if (node_id == SINK_ID) {
	  etimer_set(&et, CLOCK_SECOND * 2);
  } else {
	  etimer_set(&et, CLOCK_SECOND * 5);
  }

  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  samu_start(SLOT_DURATION, TIMEOUT, (samu_hl_cb)main_thread);

  PROCESS_END();
}

