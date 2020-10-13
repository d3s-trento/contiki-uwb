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
 * \file      A simple test for the TSM driver
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 */

#include <stdio.h>

#include "contiki.h"
#include "dw1000-config.h"
#include "dw1000-conv.h"
#include "sys/node-id.h"
#include "deployment.h"

#include "trex.h"
#include "trex-driver.h"

#define INITIATOR_ID 1
#define PERIOD (1000000*UUS_TO_DWT_TIME_32) // around 1 second
#define GUARD (100*UUS_TO_DWT_TIME_32)
#define TIMEOUT (PERIOD - 1000*UUS_TO_DWT_TIME_32)


PROCESS(trexd_test, "Trex driver test");
AUTOSTART_PROCESSES(&trexd_test);

static struct pt pt;

static struct {
  uint32_t seqn;
} pkt;

static char main_thread(const trexd_slot_t* slot) {
  static uint32_t tref;
  static int res;
  PT_BEGIN(&pt);

  printf("Starting protothread...\n");
  if (node_id == INITIATOR_ID) {
    tref = dwt_readsystimestamphi32() + PERIOD;
    while (1) {
      res = trexd_tx_at((uint8_t*)&pkt, sizeof(pkt), tref);
      PT_YIELD(&pt);

      printf("TX %lu %u %lu\n", tref, res, pkt.seqn);

      pkt.seqn++;
      tref += PERIOD;
    }
  }
  else { /* receiver */
    printf("Scanning...\n");
    do {
      // scan until receive anything
      trexd_rx((uint8_t*)&pkt);
      PT_YIELD(&pt);
      printf("Scanning RX event %lu %u %u\n", tref, res, slot->status);
    } while (slot->status != TREX_RX_SUCCESS);

    do {
      if (slot->status == TREX_RX_SUCCESS) {
        tref = slot->trx_sfd_time_4ns + PERIOD;
        printf("RX %lu %u %u %lu\n", tref, res, slot->payload_len, pkt.seqn);
      }
      else {
        tref = tref + PERIOD;
        printf("RX fail %lu %u %u\n", tref, res, slot->status);
      }

      trexd_stats_print();
      
      // TODO: size of the RX buffer is currently unchecked
      res = trexd_rx_slot((uint8_t*)&pkt, tref-GUARD, tref+TIMEOUT);
      PT_YIELD(&pt);
    } while (1);
  }

  PT_END(&pt);
}


PROCESS_THREAD(trexd_test, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  
  trexd_init();
  trexd_set_slot_callback((trexd_slot_cb)main_thread);
  trexd_stats_reset();

  printf("Calling the main thread...\n");
  main_thread(NULL);
  

  PROCESS_END();
}
