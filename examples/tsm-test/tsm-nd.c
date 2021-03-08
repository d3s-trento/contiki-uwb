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
 * \file      A simple neighbor discovery example based on the
 *            Time Slot Manager (TSM)
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 */


#include <stdio.h>
#include <string.h>
#include "contiki.h"

#include "contiki.h"
#include "lib/random.h"  // Contiki random
#include "sys/node-id.h"
#include "deployment.h"
#include "trex-tsm.h"
#include "trex-driver.h"
#include "trex.h"
#include "dw1000-conv.h"

#define LOG_PREFIX "nd"
#include "logging.h"

#define INITIATOR_ID 1

#define PERIOD (1000000*UUS_TO_DWT_TIME_32)               // ~ 1 s
#define SLOT_DURATION (1000*UUS_TO_DWT_TIME_32)           // ~ 1 ms
#define TIMEOUT (SLOT_DURATION - 400*UUS_TO_DWT_TIME_32)  // slot timeout

// Upper boundary for random TX jitter
// (if there is no jitter, it is likely that always the same device is heard)
// Make sure to disable or increase the preamble timeout if you use TX jitter
#define MAX_JITTER (6)
#define JITTER_STEP (20*UUS_TO_DWT_TIME_32)

#define MAX_NODES 20          // max number of registered neighbors
#define MAX_SLOTS 50          // max number of slots in an epoch

#define N_EMPTY_TO_SLEEP 1    // number of consecutive slots without reception to go to sleep


#define PA tsm_prev_action
#define NA tsm_next_action


static struct pt pt;    // protothread object

static struct {         // packet header
  uint16_t epoch;
  uint8_t node_id;
} __attribute__((packed)) header;

static uint8_t buffer[127];     // buffer for TX and RX
#define BUF_HEADER (buffer+TSM_HDR_LEN)
#define BUF_LIST (BUF_HEADER+sizeof(header))
static uint8_t nbr[MAX_NODES];  // neighbor list (used by the initiator only)
static uint8_t n_nbr;           // number of neighbors in the list

// does the list contain the given ID ?
bool has_nbr(const uint8_t* list, uint8_t n_nbr, uint8_t id) {
  for (int i=0; i<n_nbr; i++) {
    if (list[i] == id)
      return 1;
  }
  return 0;
}

// add a node ID if it is not present in the neighbor list
void add_nbr(uint8_t id) {
  if (!has_nbr(nbr, n_nbr, id)) {
    nbr[n_nbr] = id;
    n_nbr ++;
  }
}

// print current neighbor list
void print_nbr() {
  for (int i=0; i<n_nbr; i++) {
    printf(" %hhu", nbr[i]);
  }
  printf("\n");
}

static int n_empty_slots;   // number of consecutive empty slots registered

static char initiator_thread() {
  static uint16_t epoch;
  PT_BEGIN(&pt);
  
  while (1) {
    n_nbr = 0; // forget all discovered neighbors
    n_empty_slots = 0;

    logging_context = epoch; // just to know that the log records belong to the current epoch

    while (n_empty_slots < N_EMPTY_TO_SLEEP && PA.slot_idx < MAX_SLOTS) {
      header.epoch   = epoch;
      header.node_id = node_id;
      memcpy(BUF_HEADER, &header, sizeof(header));
      memcpy(BUF_LIST, nbr, n_nbr);
      
      TSM_TX_SLOT(&pt, buffer, sizeof(header) + n_nbr);
      TSM_RX_SLOT(&pt, buffer);

      if (PA.status == TREX_RX_SUCCESS) {
        memcpy(&header, BUF_HEADER, sizeof(header));
        add_nbr(header.node_id);
        PRINT("RX [%u:%d] from:%u len:%u remote:[%u:%d]", epoch, PA.slot_idx, header.node_id, PA.payload_len, header.epoch, PA.remote_slot_idx);
      }

      if (PA.status == TREX_RX_TIMEOUT) { // silence
        n_empty_slots ++;
        PRINT("Timeout, slot %hd", PA.slot_idx);
      }
      else { // heard at least something
        n_empty_slots = 0;
      }
    }
    printf("Epoch end %d, slots %d, nbr:", epoch, PA.slot_idx);
    print_nbr();
    trexd_stats_print(); trexd_stats_reset();
    TSM_RESTART(&pt, PERIOD); // restart from the next epoch
    epoch++;
  }
  PT_END(&pt);
}

static char responder_thread() {
  static int epoch;
  static bool bootstrapped;
  static bool acked;
  PT_BEGIN(&pt);
  
  // listen until we get any good packet
  while (1) {
    TSM_SCAN(&pt, buffer);

    if (PA.status == TREX_RX_SUCCESS) {
      PRINT("+++ Synched! Slot %hd %hd", PA.slot_idx, PA.remote_slot_idx);
      break;
    }
  }
  
  // here we reach after hearing a good packet (initiator or another responder)

  while (1) {
    acked = 0;
    n_empty_slots = 0;

    logging_context = epoch; // just to know that the log records belong to the current epoch

    while (1) {
      if (!bootstrapped) {
        if (PA.remote_slot_idx % 2) { // got a packet from another responder
          // listen again, a packet from the initiator should arrive
          TSM_RX_SLOT(&pt, buffer);
        }
        bootstrapped = 1; // at this point we are aligned with the slot structure
      }
      else { // already bootstrapped
        TSM_RX_SLOT(&pt, buffer);
      }

      if (PA.status == TREX_RX_SUCCESS) {
        memcpy(&header, BUF_HEADER, sizeof(header));
        acked = has_nbr(BUF_LIST, PA.payload_len - sizeof(header), node_id);
        epoch = header.epoch;
        NA.accept_sync = true;    // resynch with the last packet
        PRINT("RX [%u:%u] from:%u len:%u acked:%u remote:[%u:%d]", epoch, PA.slot_idx, header.node_id, PA.payload_len, acked, header.epoch, PA.remote_slot_idx);
      }

      if (PA.status == TREX_RX_TIMEOUT) { // silence
        PRINT("Timeout, slot %hd", PA.slot_idx);
        n_empty_slots ++;
      }
      else { // heard at least something
        n_empty_slots = 0;
      }
      if (acked || n_empty_slots >= N_EMPTY_TO_SLEEP) {
        break;
      }
      header.epoch = epoch;
      header.node_id = node_id;
      memcpy(BUF_HEADER, &header, sizeof(header));
      NA.tx_delay = MAX_JITTER ? ((random_rand() % (MAX_JITTER+1))*JITTER_STEP) : 0; // add random delay

      TSM_TX_SLOT(&pt, buffer, sizeof(header));
    }

    trexd_stats_print(); trexd_stats_reset();
    TSM_RESTART(&pt, PERIOD); // restart from the next epoch
    epoch ++;
  }
  PT_END(&pt);
}


PROCESS(tsm_nd_test, "Trex slot machine test: simple neighbor discovery");
AUTOSTART_PROCESSES(&tsm_nd_test);
PROCESS_THREAD(tsm_nd_test, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  tsm_init();
  tsm_set_default_preambleto(0); // disable preamble timeout

  if (node_id == INITIATOR_ID) {
    tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)initiator_thread);
  }
  else {
    tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)responder_thread);
  }

  PROCESS_END();
}
