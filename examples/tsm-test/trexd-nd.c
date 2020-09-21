#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "lib/random.h"  // Contiki random
#include "dw1000-config.h"
#include "dw1000-conv.h"
#include "sys/node-id.h"
#include "deployment.h"
#include "trex.h"
#include "trex-driver.h"

#define INITIATOR_ID 1

#define PERIOD (1000000*UUS_TO_DWT_TIME_32)               // ~ 1 s
#define SLOT_DURATION (1000*UUS_TO_DWT_TIME_32)           // ~ 1 ms
#define GUARD (100*UUS_TO_DWT_TIME_32)                    // receivers guard time
#define TIMEOUT (SLOT_DURATION - 400*UUS_TO_DWT_TIME_32)  // slot timeout

#define MAX_JITTER 6
#define JITTER_STEP_UUS 20

#define MAX_NODES 20          // max number of registered neighbors
#define MAX_SLOTS 50          // max number of slots in an epoch

#define N_EMPTY_TO_SLEEP 1    // number of consecutive slots without reception to go to sleep


static struct pt pt;    // protothread object

static struct {         // packet header
  uint16_t epoch;
  uint16_t slot;
  uint8_t node_id;
  uint8_t delay_uus;
} header;


static uint8_t buffer[127];     // buffer for TX and RX
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

static uint32_t tref;       // sfd of slot 0 of the current epoch
static uint32_t slot_ts;    // sfd of the current slot
static uint16_t epoch;      // current epoch index
static uint16_t slot_idx;   // current slot index
static int res;             // holds request return value
static int n_empty_slots;   // number of consecutive empty slots registered

static char initiator_thread(const trexd_slot_t* slot) {
  PT_BEGIN(&pt);

  printf("Starting initiator protothread...\n");
  tref = dwt_readsystimestamphi32() + PERIOD;
  epoch = 0;
  while (1) {
    slot_idx = 0;
    slot_ts = tref; // sfd of slot 0 is the tref of the epoch
    n_empty_slots = 0;
    n_nbr = 0; // forget all discovered neighbors

    do {
      // we start from a TX slot
      header.epoch   = epoch;
      header.slot    = slot_idx;
      header.node_id = node_id;
      header.delay_uus = 0; // initiator never jitters

      memcpy(buffer, &header, sizeof(header));
      memcpy(buffer + sizeof(header), nbr, n_nbr);

      res = trexd_tx_at(buffer, sizeof(header) + n_nbr, slot_ts);
      printf("TX [%u:%u] res:%u %lu\n", epoch, slot_idx, res, tref);
      PT_YIELD(&pt);

      slot_idx ++;
      slot_ts += SLOT_DURATION;

      res = trexd_rx_slot(buffer, slot_ts-GUARD, slot_ts+TIMEOUT);
      PT_YIELD(&pt);

      // we follow with an RX slot
      if (slot->status == TREX_RX_SUCCESS) {
        memcpy(&header, buffer, sizeof(header));
        add_nbr(header.node_id); // add neighbor to the list
        printf("RX [%u:%u] from:%u len:%u dly %u st:%u %lu\n", epoch, slot_idx, header.node_id, slot->payload_len, header.delay_uus, slot->status, tref);
        n_empty_slots = 0;
      }
      else if (slot->status == TREX_RX_TIMEOUT) {
        n_empty_slots ++; // only increment in case of timeout (i.e., no preamble detected)
      }
      else {
        n_empty_slots = 0; // we heard a preamble but failed to RX, probably someone was sending
      }

      slot_idx ++;
      slot_ts += SLOT_DURATION;

    } while(n_empty_slots < N_EMPTY_TO_SLEEP && slot_idx < MAX_SLOTS);

    printf("Epoch %d, slots %u, nbr:", epoch, slot_idx);
    print_nbr();

    tref += PERIOD;
    epoch ++;
  }
  PT_END(&pt);
}


static char responder_thread(const trexd_slot_t* slot) {
  static int acked;
  static int bootstrapped;
  PT_BEGIN(&pt);

  while (1) {
    slot_ts = tref; // sfd of slot 0 is the tref of the epoch
    slot_idx = 0;
    while (1) {
      if (!bootstrapped) {
        printf("Scanning...\n");
        res = trexd_rx(buffer);
      }
      else /*bootstrapped*/ {
        // TODO: size of the RX buffer is currently unchecked
        res = trexd_rx_slot(buffer, slot_ts-GUARD, slot_ts+TIMEOUT);
      }

      PT_YIELD(&pt);

      if (slot->status == TREX_RX_SUCCESS) {
        memcpy(&header, buffer, sizeof(header));
        epoch = header.epoch;
        slot_idx = header.slot;
        slot_ts = slot->trx_sfd_time_4ns - header.delay_uus*UUS_TO_DWT_TIME_32;
        tref = slot->trx_sfd_time_4ns - (header.slot*SLOT_DURATION);

        // are we in the list of known neighbors?
        acked = has_nbr(buffer+sizeof(header), slot->payload_len - sizeof(header), node_id);
        n_empty_slots = 0;
        bootstrapped = 1;
        printf("RX [%u:%u] from:%u len:%u acked:%u dly:%u st:%u %lu\n", epoch, slot_idx, header.node_id, slot->payload_len, acked, header.delay_uus, slot->status, tref);
      }
      else {
        printf("RX fail [%u:%u] st:%u %lu\n", epoch, slot_idx, slot->status, tref);
        n_empty_slots ++;
      }

      if (n_empty_slots >= N_EMPTY_TO_SLEEP || acked || slot_idx > MAX_SLOTS) {
        break; // stop the current epoch
      }

      if (bootstrapped) {
        slot_idx ++;
        slot_ts += SLOT_DURATION;

        if (slot_idx % 2) { // odd slot, responders TX (after bootstrap)
          header.epoch   = epoch;
          header.slot    = slot_idx;
          header.node_id = node_id;
          header.delay_uus = MAX_JITTER ? ((random_rand() % (MAX_JITTER+1))*JITTER_STEP_UUS) : 0;
          
          memcpy(buffer, &header, sizeof(header));
        
          res = trexd_tx_at(buffer, sizeof(header), slot_ts + header.delay_uus * UUS_TO_DWT_TIME_32);
          printf("TX [%u:%u] dly:%u res:%u %lu\n", epoch, slot_idx, header.delay_uus, res, tref);
          PT_YIELD(&pt);

          slot_idx ++;
          slot_ts += SLOT_DURATION;
        }
      }

      //trexd_stats_print();

    }
    tref = tref + PERIOD;
    epoch ++;
  }
  PT_END(&pt);
}


PROCESS(trexd_nd_test, "Trex driver test: simple neighbor discovery");
AUTOSTART_PROCESSES(&trexd_nd_test);
PROCESS_THREAD(trexd_nd_test, ev, data)
{
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();
  trexd_init();
  trexd_stats_reset();

  if (node_id == INITIATOR_ID) {
    trexd_set_slot_callback((trexd_slot_cb)initiator_thread);
    initiator_thread(NULL);
  }
  else {
    trexd_set_slot_callback((trexd_slot_cb)responder_thread);
    responder_thread(NULL);
  }

  PROCESS_END();
}
