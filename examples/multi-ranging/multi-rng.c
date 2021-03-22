/* Copyright (c) 2021, University of Trento.
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
 */

/**
 * \file
 *      Multi-Ranging with Master Tag
 *
 * \author
 *      Davide Vecchia <davide.vecchia@unitn.it>
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "dw1000.h"
#include "dw1000-ranging.h"
#include "dw1000-util.h"
#include "dw1000-cir.h"
#include "dw1000-config.h"
#include "core/net/linkaddr.h"
/*--------------------------------------------------------------------------*/
#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while(0)
#endif

/*-- Configuration ----------------------------------------------------------*/
linkaddr_t master_tag = {{0x18, 0x32}}; // 11 // orchestrates other tags
linkaddr_t other_tags[] = { // tags, wait for the schedule from the master (may be empty)
}; 
linkaddr_t anchors[] = { // responders (a node can be both a tag and an anchor)
  {{0x10, 0x89}}, // 8
  {{0x11, 0x4a}}, // 9
  {{0xc8, 0x0c}}, // 10
  {{0x18, 0x32}}, // 11
  {{0x17, 0x93}}, // 12
  {{0x02, 0xd8}}, // 13
  {{0xca, 0x99}}, // 14
  {{0x14, 0x53}}, // 15
  {{0x51, 0x08}}, // 16
  {{0x15, 0x81}}, // 17
  //{{0xaa, 0xaa}}, // invalid node for debugging
};
#define RANGING_STYLE  DW1000_RNG_SS     // single- or double-sided (DW1000_RNG_DS)
#define ROUND_PERIOD   (CLOCK_SECOND*1)  // period of multi-ranging

#define ACQUIRE_CIR 1           // 1 = enable CIR acquisition
#define CIR_START_FROM_PEAK 0   // 0 = print from beginning, 1 = print starting from the first ray peak
#define CIR_MAX_SAMPLES DW1000_CIR_MAX_LEN // number of CIR samples to acquire
#define PRINT_RXDIAG 1          // 1 = enable printing RX diagnostics

/*--------------------------------------------------------------------------*/
#if ACQUIRE_CIR
#define MAX_PRINTING_TIME (CLOCK_SECOND / 20)     // estimated time needed to print a full CIR (depends on the platform)
#define CIR_DUMP_DELAY    (CLOCK_SECOND / 200)    // a delay inserted after printing CIR to let the USB buffer get emptied
#else
#define MAX_PRINTING_TIME (CLOCK_SECOND / 1000)   // time needed to print ranging results and diagnostics (upper bound)
#define CIR_DUMP_DELAY 0
#endif
/*--------------------------------------------------------------------------*/
#define INIT_GUARD 2 // leave one-two ticks between the command tx/rx and the first ranging slot
#define RANGING_GAP 2 // leave one-two ticks between consecutive rangings
#if RANGING_STYLE == DW1000_RNG_SS
#define RANGING_TIME (CLOCK_SECOND / 1000)  // time allocated for a single TWR
#else
#define RANGING_TIME (CLOCK_SECOND / 500)
#endif
#define TOTAL_RANGING_TIME (RANGING_TIME + MAX_PRINTING_TIME + CIR_DUMP_DELAY + RANGING_GAP)
#define NUM_OTHER_TAGS (sizeof(other_tags) / sizeof(other_tags[0]))
#define NUM_ANCHORS (sizeof(anchors) / sizeof(anchors[0]))

#define TAG_SLOT_DURATION (TOTAL_RANGING_TIME*NUM_ANCHORS)
#define TOTAL_ROUND_DURATION (TAG_SLOT_DURATION*(NUM_OTHER_TAGS+1) + INIT_GUARD)

// sanity check that there is enough time for ranging
_Static_assert (ROUND_PERIOD > TOTAL_ROUND_DURATION + CLOCK_SECOND/100, 
                "Not enough time for ranging");

/*--------------------------------------------------------------------------*/
#define ROLE_TAG_MASTER 1
#define ROLE_TAG 2
#define ROLE_ANCHOR 3
/*--------------------------------------------------------------------------*/

#if ACQUIRE_CIR
dw1000_cir_sample_t cir_buf[CIR_MAX_SAMPLES+1]; // +1 is required!
#endif

#if PRINT_RXDIAG
void
print_rxdiag(const dwt_rxdiag_t *d, const dw1000_rxpwr_t *p) {
  printf("fpa:%u,%u,%u cir_pwr(raw):%d(%u) pac(nonsat):%u(%u) max_noise:%u std_noise:%u\n",
    d->firstPathAmp1,
    d->firstPathAmp2,
    d->firstPathAmp3,
    (int)p->cir_pwr,
    d->maxGrowthCIR,
    d->rxPreamCount,
    d->pacNonsat,
    d->maxNoise,
    d->stdNoise
  );
}
#endif

PROCESS(ranging_process, "Ranging process");
AUTOSTART_PROCESSES(&ranging_process);
/*--------------------------------------------------------------------------*/
static process_event_t master_control_event;
static uint32_t seqn;
static bool busy_ranging;
static uint8_t tag_slot;
/*--------------------------------------------------------------------------*/
static void bc_recv(struct broadcast_conn *c, const linkaddr_t *from);
static void bc_sent(struct broadcast_conn *c, int status, int num_tx);
static const struct broadcast_callbacks broadcast_call = {bc_recv, bc_sent};
static bool fill_ctrl_packet();
static struct broadcast_conn broadcast;
/*--------------------------------------------------------------------------*/
PROCESS_THREAD(ranging_process, ev, data)
{
  static struct etimer et, et_slot;
  static uint8_t role;
  static int i, status;

  PROCESS_BEGIN();

  master_control_event = process_alloc_event();
  seqn = 0;

  etimer_set(&et, 2*CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  printf("I am %02x%02x master_tag %02x%02x #tags %u #anchors %u\n",
         linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
         master_tag.u8[0], master_tag.u8[1], NUM_OTHER_TAGS, NUM_ANCHORS);
  dw1000_print_cfg();

  /* Create broadcast connection for control messages,
   * and set the appropriate role for each node */
  role = ROLE_ANCHOR;
  if(linkaddr_cmp(&linkaddr_node_addr, &master_tag)) {
    broadcast_open(&broadcast, 129, &broadcast_call);
    etimer_set(&et, ROUND_PERIOD);
    printf("Role: master_tag\n");
    role = ROLE_TAG_MASTER;
  }
  else {
    for(i=0; i<NUM_OTHER_TAGS; i++) {
      if(linkaddr_cmp(&linkaddr_node_addr, &other_tags[i])) {
        broadcast_open(&broadcast, 129, &broadcast_call);
        printf("Role: tag\n");
        role = ROLE_TAG;
        break;
      }
    }
    if(role == ROLE_ANCHOR) {
      printf("Role: anchor\n");
    }
  }

  if (role == ROLE_TAG_MASTER || role == ROLE_TAG) {
    /* Ranging loop for tags */
    while(1) {

      /* The master tag sends a control message embedding all
       * tags in the order they must use to perform ranging;
       * other tags wait for the control message */
      if(role == ROLE_TAG_MASTER) {
        PROCESS_YIELD_UNTIL(etimer_expired(&et));
        etimer_set(&et, ROUND_PERIOD);
        if (NUM_OTHER_TAGS > 0) { // only send the command if there are other tags
          PRINTF("Sending control %lu\n", seqn);
          bool res = fill_ctrl_packet();
          if (res) {
            broadcast_send(&broadcast);
          }
          // wait for the broadcast to complete
          PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
        }
        tag_slot = 0; // master always has slot 0
        seqn++;
      }
      else if(role == ROLE_TAG) {
        PROCESS_YIELD_UNTIL(ev == master_control_event);
        // tag_slot was filled according to the received packet
      }

      busy_ranging = 1; // mark that we're busy till the end of the round
      PRINTF("Prepare ranging [%lu] (slot: %u)\n", seqn, tag_slot);

      /* Compute waiting time to let other tags do ranging */
      clock_time_t tag_wait = INIT_GUARD + tag_slot * TAG_SLOT_DURATION;
      if(tag_wait != 0) {
        etimer_set(&et_slot, tag_wait);
        PROCESS_WAIT_UNTIL(etimer_expired(&et_slot));
      }

      static clock_time_t slot_start;
      slot_start = clock_time();

      /* Range with each anchor */
      for(i=0; i<NUM_ANCHORS; i++) {
        static linkaddr_t dst;
        dst = anchors[i];
        if(linkaddr_cmp(&linkaddr_node_addr, &dst)) continue;
        printf("RNG [%lu/%lums] %02x%02x->%02x%02x: ",
            seqn, (clock_time() * 1000UL / CLOCK_SECOND),
            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
            dst.u8[0], dst.u8[1]);
#if ACQUIRE_CIR
        dw1000_ranging_acquire_diagnostics(
            CIR_START_FROM_PEAK ? DW1000_GET_CIR_FROM_FP : 0,
            CIR_MAX_SAMPLES,
            cir_buf);
#else // only request diagnostics
        dw1000_ranging_acquire_diagnostics(0, 0, NULL);
#endif
        status = range_with(&dst, RANGING_STYLE);
        if(!status) {
          printf("REQ FAIL\n");
        }
        else {
          PROCESS_YIELD_UNTIL(ev == ranging_event);
          if(((ranging_data_t *)data)->status) {
            static ranging_data_t *d;
            static dw1000_rxpwr_t rxpwr;

            d = data;
            dw1000_rxpwr(&rxpwr, &d->rxdiag, dw1000_get_current_cfg());
            printf("SUCCESS %d bias %d fppwr %d rxpwr %d cifo %d\n",
                (int)(100*d->raw_distance), (int)(100*d->distance),
                (int)(1000*rxpwr.fp_pwr), (int)(1000*rxpwr.rx_pwr),
                (int)(10e8*d->freq_offset));
#if ACQUIRE_CIR
            uint16_t cir_start = cir_buf[0];
            uint16_t cir_fp_int = d->rxdiag.firstPath >> 6;
            uint16_t cir_fp_frac = d->rxdiag.firstPath & 0x3f;
            // fp (first path) is printed as the integer part and the 1/64 fractional part
            // "fp a#b" means (a + b/64)
            printf("CIR [%lu] %02x%02x->%02x%02x %d#%d [%d:%d] ", 
                seqn,
                linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                dst.u8[0], dst.u8[1],
                cir_fp_int, cir_fp_frac, cir_start, d->cir_samples_acquired);
            dw1000_print_cir_hex(cir_buf+1, d->cir_samples_acquired);
            if (CIR_DUMP_DELAY != 0) {
              etimer_set(&et_slot, CIR_DUMP_DELAY);
              PROCESS_WAIT_UNTIL(etimer_expired(&et_slot));
            }
#endif
#if PRINT_RXDIAG
            printf("DIAG [%lu] %02x%02x->%02x%02x ", 
                seqn,
                linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                dst.u8[0], dst.u8[1]);
            print_rxdiag(&d->rxdiag, &rxpwr);
#endif
          }
          else {
            printf("FAIL\n");
          }
        }

        if (RANGING_GAP > 0) {
          // add a gap between two rangings
          etimer_set(&et_slot, RANGING_GAP);
          PROCESS_WAIT_UNTIL(etimer_expired(&et_slot));
        }
      }

      if (clock_time() - slot_start > TAG_SLOT_DURATION) {
        printf("Error: tag slot duration exceeded, adjust the timing to avoid collisions!\n");
      }
      busy_ranging = 0;
    }
  }

  PROCESS_END();
}


static bool 
fill_ctrl_packet() {
  packetbuf_clear();
  uint8_t* payload = packetbuf_dataptr();
  int required_len = 11 + sizeof(linkaddr_t)*NUM_OTHER_TAGS;
  if (packetbuf_remaininglen() < required_len) {
    printf("Error: too big command payload\n");
    return false;
  }
 
  // max duration of tag's ranging session with all anchors in clock ticks
  uint32_t max_duration = TAG_SLOT_DURATION;

  memcpy(&payload[0], &seqn, sizeof(seqn));
  memcpy(&payload[4], &max_duration, sizeof(max_duration));
  payload[8] = NUM_OTHER_TAGS + 1;
  payload[9] = linkaddr_node_addr.u8[0];
  payload[10] = linkaddr_node_addr.u8[1];
  for(int i=0; i<NUM_OTHER_TAGS; i++) {
    memcpy(&payload[11+sizeof(linkaddr_t)*i], &other_tags[i], sizeof(linkaddr_t));
  }
  packetbuf_set_datalen(required_len);
  return true;
}

/*--------------------------------------------------------------------------*/
static void
bc_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  bool found_myself = false;

  if(linkaddr_cmp(&master_tag, from)) {
    uint8_t* bc_data = packetbuf_dataptr();
    memcpy(&seqn, bc_data, sizeof(seqn));
    uint32_t max_duration;
    memcpy(&max_duration, &bc_data[4], sizeof(max_duration));

    uint8_t num_ordered_tags = bc_data[8];
    linkaddr_t ordered_tags[num_ordered_tags];
    memcpy(&ordered_tags, &bc_data[9], num_ordered_tags*sizeof(linkaddr_t));
    PRINTF("Control received (seqn: %lu, dur: %lu tags: #%u >", seqn, max_duration, num_ordered_tags);
    if (busy_ranging) {
      printf("Error: received a new command while busy with the previous one, adjust the timing to avoid collisions!\n");
      return;
    }

    for(int i=0; i<num_ordered_tags; i++) {
      linkaddr_t* tag = &ordered_tags[i];
      PRINTF("%02x%02x ", tag->u8[0], tag->u8[1]);
      if(linkaddr_cmp(&linkaddr_node_addr, tag)) {
        tag_slot = i;
        found_myself = true;
      }
    }
    PRINTF(")\n");

    if (TAG_SLOT_DURATION > max_duration) {
      printf("Error: too short time allowed (firmware mismatch?)\n");
      return;
    }

    if (found_myself) {
      process_post(&ranging_process, master_control_event, NULL);
    }
  }
  else {
    printf("Unexpected broadcast.\n");
  }
}
/*--------------------------------------------------------------------------*/
static void
bc_sent(struct broadcast_conn *c, int status, int num_tx) {
  if(DEBUG) {
    if (status == MAC_TX_OK)
      PRINTF("Control TX ok %d [%lums]\n", num_tx, clock_time() * 1000UL / CLOCK_SECOND);
    else
      PRINTF("Control TX failed %d\n", num_tx);
  }
  process_poll(&ranging_process);
}
/*--------------------------------------------------------------------------*/
