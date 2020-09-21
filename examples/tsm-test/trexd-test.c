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
