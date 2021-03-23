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
 * \file      A simple Time Slot Manager (TSM) test spawning subthreads
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
#include "trex.h"
#include "dw1000-conv.h"

#define PERIOD (1000000*UUS_TO_DWT_TIME_32)               // ~ 1 s
#define SLOT_DURATION (3000*UUS_TO_DWT_TIME_32)           // ~ 1 ms
#define TIMEOUT (SLOT_DURATION - 1000*UUS_TO_DWT_TIME_32) // slot timeout

static struct { // packet header
  uint16_t epoch;
  uint8_t node_id;
} header;

static uint8_t buffer[127];   // buffer for TX and RX


static struct pt inner_pt;    // protothread object
static char inner_thread() {
  PT_BEGIN(&inner_pt);

  static int start_slot; // make sure not to initialise static variables inline
 
  start_slot = tsm_prev_action.slot_idx;
  do {
    memcpy(buffer+TSM_HDR_LEN, &header, sizeof(header));

    TSM_TX_SLOT(&inner_pt, buffer, sizeof(header));
    if (tsm_prev_action.slot_idx == 0) {
      tsm_next_action.progress_slots = 2; // skip a slot
    }
  } while (tsm_prev_action.slot_idx < start_slot + 9);

  PT_END(&inner_pt);
}


static struct pt outer_pt;    // protothread object
static char outer_thread() {
  static uint16_t epoch;
  PT_BEGIN(&outer_pt);
  
  while (1) {
    header.epoch   = epoch;
    header.node_id = node_id;
    printf("epoch %hu\n", epoch);

    PT_SPAWN(&outer_pt, &inner_pt, inner_thread());
    PT_SPAWN(&outer_pt, &inner_pt, inner_thread());

    TSM_RESTART(&outer_pt, PERIOD);
    epoch ++;
  }
  PT_END(&outer_pt);
}

PROCESS(tsm_test, "Trex slot machine test");
AUTOSTART_PROCESSES(&tsm_test);
PROCESS_THREAD(tsm_test, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  tsm_init();
  tsm_set_default_preambleto(0); // disable preamble timeout

  tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)outer_thread);

  PROCESS_END();
}
