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
#include "dw1000-diag.h"
#include "dw1000-config.h"
#include "core/net/linkaddr.h"
/*--------------------------------------------------------------------------*/
PROCESS(ranging_process, "Ranging process");
AUTOSTART_PROCESSES(&ranging_process);
/*--------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while(0)
#endif
/*--------------------------------------------------------------------------*/
#define RANGING_STYLE DW1000_RNG_SS // single- or double-sided (DW1000_RNG_DS)
#define RANGING_INTERVAL (CLOCK_SECOND) // period of multi-ranging
#define RANGING_DELAY (CLOCK_SECOND / 200) // between each TWR in a series
#if RANGING_STYLE == DW1000_RNG_SS
#define RANGING_TIME (CLOCK_SECOND / 100) // time allocated for a single TWR
#else
#define RANGING_TIME (CLOCK_SECOND / 50)
#endif
#define TOTAL_RANGING_TIME (RANGING_TIME + RANGING_DELAY)
/*--------------------------------------------------------------------------*/
linkaddr_t master_tag = {{0x13, 0x9a}}; // orchestrates other tags
linkaddr_t other_tags[] = { // tags wait for the schedule from the master
  {{0x19, 0x15}},
  {{0x11, 0xa3}},
  {{0x11, 0x0c}},
  {{0x12, 0x8a}},
  {{0x10, 0x9b}},
  {{0x18, 0x33}},
}; 
linkaddr_t anchors[] = { // responders (a node can be both a tag and an anchor)
  {{0x13, 0x9a}},
  {{0x19, 0x15}},
  {{0x11, 0xa3}},
  {{0x11, 0x0c}},
  {{0x12, 0x8a}},
  {{0x10, 0x9b}},
  {{0x18, 0x33}},
};
#define ROLE_TAG_MASTER 1
#define ROLE_TAG 2
#define ROLE_ANCHOR 3
#define NUM_OTHER_TAGS (sizeof(other_tags) / sizeof(other_tags[0]))
#define NUM_ANCHORS (sizeof(anchors) / sizeof(anchors[0]))
/*--------------------------------------------------------------------------*/
static process_event_t master_control_event;
static uint32_t seqn;
static uint8_t tag_slot;
/*--------------------------------------------------------------------------*/
static void bc_recv(struct broadcast_conn *c, const linkaddr_t *from);
static void bc_sent(struct broadcast_conn *c, int status, int num_tx);
static const struct broadcast_callbacks broadcast_call = {bc_recv, bc_sent};
static struct broadcast_conn broadcast;
/*--------------------------------------------------------------------------*/
PROCESS_THREAD(ranging_process, ev, data)
{
  static struct etimer et, et_rng;
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
    etimer_set(&et, RANGING_INTERVAL);
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

  /* Ranging loop for tags */
  while(1) {

    /* The master tag sends a control message embedding all
     * tags in the order they must use to perform ranging;
     * other tags wait for the control message */
    if(role == ROLE_TAG_MASTER) {
      PROCESS_YIELD_UNTIL(etimer_expired(&et));
      etimer_set(&et, RANGING_INTERVAL);
      PRINTF("Sending control %lu\n", seqn);
      packetbuf_clear();
      uint8_t* payload = packetbuf_dataptr();
      memcpy(&payload[0], &seqn, sizeof(seqn));
      payload[4] = NUM_OTHER_TAGS + 1;
      payload[5] = linkaddr_node_addr.u8[0];
      payload[6] = linkaddr_node_addr.u8[1];
      for(i=0; i<NUM_OTHER_TAGS; i++) {
        payload[7+2*i] = other_tags[i].u8[0];
        payload[8+2*i] = other_tags[i].u8[1];
      }
      packetbuf_set_datalen(7+2*NUM_OTHER_TAGS);
      broadcast_send(&broadcast);
      PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
      seqn++;
    }
    else if(role == ROLE_TAG) {
      PROCESS_YIELD_UNTIL(ev == master_control_event);

      /* Compute waiting time to let other tags do ranging */
      clock_time_t tag_wait = tag_slot * TOTAL_RANGING_TIME * NUM_ANCHORS;
      if(tag_wait != 0) {
        etimer_set(&et, tag_wait);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));
      }
    }
    else break;

    /* Range with each anchor */
    for(i=0; i<NUM_ANCHORS; i++) {
      linkaddr_t dst = anchors[i];
      if(linkaddr_cmp(&linkaddr_node_addr, &dst)) continue;
      printf("RNG [%lu/%lums] %02x%02x->%02x%02x: ",
        seqn, (clock_time() * 1000UL / CLOCK_SECOND),
        linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
        dst.u8[0], dst.u8[1]);
      status = range_with(&dst, RANGING_STYLE);
      if(!status) {
        printf("REQ FAIL\n");
      }
      else {
        PROCESS_YIELD_UNTIL(ev == ranging_event);
        if(((ranging_data_t *)data)->status) {
          ranging_data_t *d = data;
          dw1000_diagnostics_t diag;
          dw1000_diagnostics(&diag, dw1000_get_current_cfg());
          printf("SUCCESS %d bias %d fppwr %d rxpwr %d cifo %d\n",
            (int)(100*d->raw_distance), (int)(100*d->distance),
            (int)(1000*diag.fp_pwr), (int)(1000*diag.rx_pwr),
            (int)(10e8*d->freq_offset));
        }
        else {
          printf("FAIL\n");
        }
      }
      if(RANGING_DELAY > 0) {
        etimer_set(&et_rng, RANGING_DELAY);
        PROCESS_WAIT_UNTIL(etimer_expired(&et_rng));
      }
    }
  }

  PROCESS_END();
}
/*--------------------------------------------------------------------------*/
static void
bc_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  if(linkaddr_cmp(&master_tag, from)) {
    uint8_t* bc_data = packetbuf_dataptr();
    memcpy(&seqn, bc_data, sizeof(seqn));
    uint8_t num_ordered_tags = bc_data[5];
    linkaddr_t ordered_tags[num_ordered_tags];
    memcpy(&ordered_tags, &bc_data[5], num_ordered_tags*sizeof(linkaddr_t));
    PRINTF("Control received (tags: #%u", num_ordered_tags);
    int i;
    for(i=0; i<num_ordered_tags; i++) {
      linkaddr_t* tag = &ordered_tags[i];
      PRINTF(" %02x%02x ", tag->u8[0], tag->u8[1]);
      if(linkaddr_cmp(&linkaddr_node_addr, tag)) {
        tag_slot = i;
        PRINTF("...)\nPrepare ranging [%lu] (slot: %u)\n",
          seqn, tag_slot);
        process_post(&ranging_process, master_control_event, NULL);
        return;
      }
    }
    PRINTF(")\n");
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
      PRINTF("Control TX ok [%lums]\n", clock_time() * 1000UL / CLOCK_SECOND);
    else
      PRINTF("Control TX failed\n");
  }
  process_poll(&ranging_process);
}
/*--------------------------------------------------------------------------*/