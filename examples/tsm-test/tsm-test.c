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

#define INITIATOR_ID 1

#define PERIOD (1000000*UUS_TO_DWT_TIME_32)               // ~ 1 s
#define SLOT_DURATION (3000*UUS_TO_DWT_TIME_32)           // ~ 1 ms
#define TIMEOUT (SLOT_DURATION - 1000*UUS_TO_DWT_TIME_32) // slot timeout

#define MAX_JITTER (6)
#define JITTER_STEP (20*UUS_TO_DWT_TIME_32)

static struct pt pt;    // protothread object

static struct {         // packet header
  uint16_t epoch;
  uint8_t node_id;
} header;

static uint8_t buffer[127];     // buffer for TX and RX

static char initiator_thread() {
  static uint16_t epoch;
  PT_BEGIN(&pt);
  
  while (1) {
    header.epoch   = epoch;
    header.node_id = node_id;
    printf("epoch %hu\n", epoch);

    do {
      memcpy(buffer+TSM_HDR_LEN, &header, sizeof(header));
      
      //printf("call tx slot\n");
      tsm_next_action.tx_delay = MAX_JITTER ? ((random_rand() % (MAX_JITTER+1))*JITTER_STEP) : 0;
      TSM_TX_SLOT(&pt, buffer, sizeof(header));
      if (tsm_prev_action.slot_idx == 0) {
        tsm_next_action.progress_slots = 2; // skip a slot
      }
      //printf("after tx slot\n");
      //TSM_RX_SLOT(&pt, buffer);
      //printf("after rx slot\n");
    } while (tsm_prev_action.slot_idx < 9);
    TSM_RESTART(&pt, PERIOD);
    epoch ++;
  }
  PT_END(&pt);
}

static char responder_thread() {
  PT_BEGIN(&pt);
  
  while (1) {
    TSM_SCAN(&pt, buffer);

    if (tsm_prev_action.status == TREX_RX_SUCCESS) {
      printf("+++ Synched! Slot %hd\n", tsm_prev_action.slot_idx);
      break;
    }
  }
  while (1) {
    printf("+++ New epoch\n");

    while (tsm_prev_action.slot_idx < 9) {
      TSM_RX_SLOT(&pt, buffer);
      if (tsm_prev_action.status == TREX_RX_SUCCESS) {
        memcpy(&header, buffer+TSM_HDR_LEN, sizeof(header));
        printf("+++ RX lsl %hd, rsl %hd, from %hhu, epoch %u\n",
            tsm_prev_action.slot_idx, tsm_prev_action.remote_slot_idx,
            header.node_id, header.epoch);
        
        // request TSM to resynchronise using the received packet
        tsm_next_action.accept_sync = 1;
      }
      else {
        printf("--- no RX lsl %hu\n", tsm_prev_action.slot_idx);
      }
      if (tsm_prev_action.slot_idx == 3) {
        tsm_next_action.progress_slots = 2; // skip a slot
      }
    }
    TSM_RESTART(&pt, PERIOD);

  }
  PT_END(&pt);
}

PROCESS(tsm_nd_test, "Trex slot machine test");
AUTOSTART_PROCESSES(&tsm_nd_test);
PROCESS_THREAD(tsm_nd_test, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  tsm_init();

  if (node_id == INITIATOR_ID) {
    tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)initiator_thread);
  }
  else {
    tsm_start(SLOT_DURATION, TIMEOUT, (tsm_slot_cb)responder_thread);
  }

  PROCESS_END();
}
