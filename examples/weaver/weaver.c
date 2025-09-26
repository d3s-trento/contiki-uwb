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
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 * \author    Matteo Trobinger    <matteo.trobinger@unitn.it>
 * \author    Davide Vecchia      <davide.vecchia@unitn.it>
 */
#include PROJECT_CONF_H

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>

#include "contiki.h"
#include "lib/random.h"  // Contiki random
#include "sys/node-id.h"
#include "print-def.h"
#include "deployment.h"
#include "trex-driver.h" // required for TREXD_FRAME_OVERHEAD
#include "trex-tsm.h"
#include "trex.h"
#include "logging.h"
#include "dw1000-conv.h"
#include "dw1000-util.h" // required for dw1000_estimate_tx_time
#include "dw1000-config.h" // dw1000_get_current_cfg


#include "weaver-utility.h"
#include "weaver-log.h"
#include "rrtable.h"

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

#define DEBUG 1
#if DEBUG
#define PRINTF(...)     printf(__VA_ARGS__)
#else
#define PRINTF(...)     do {} while(0)
#endif /* DEBUG */


#if WEAVER_LOG_VERBOSE
#define WEAVER_LOG_PRINT() weaver_log_print()
#else
#define WEAVER_LOG_PRINT() do {} while(0);
#endif // WEAVER_LOG_VERBOSE

#undef TREX_STATS_PRINT
#define TREX_STATS_PRINT()\
    do {trexd_stats_get(&trex_stats);\
        PRINTF("E %lu, TX %d, RX %d, TO %d, ER %d\n", logging_context,\
                trex_stats.n_txok, trex_stats.n_rxok,\
                (trex_stats.n_fto + trex_stats.n_pto),\
                (trex_stats.n_phe + trex_stats.n_sfdto + trex_stats.n_rse + trex_stats.n_fcse + trex_stats.n_rej));\
    } while(0)

// sink id
#ifndef SINK_ID
#define SINK_ID             1
#endif // SINK_ID

#define SINK_ID_CONSTANT    0xffff     // do not define a specific node, but rather any sink node

#ifndef WEAVER_NTX
#define WEAVER_NTX     1
#endif // WEAVER_NTX

#ifndef WEAVER_NRX
#define WEAVER_NRX     2
#endif // WEAVER_NRX


#ifndef WEAVER_SLEEP_NTX
#define WEAVER_SLEEP_NTX     1
#endif // WEAVER_SLEEP_NTX

#if WEAVER_LOG_VERBOSE
#define PERIOD_SINK (1000000 * UUS_TO_DWT_TIME_32)                   // ~ 1s
#else
#define PERIOD_SINK (500000 * UUS_TO_DWT_TIME_32)                   // ~ 500ms
#endif // WEAVER_LOG_VERBOSE

#define PERIOD_PEER (PERIOD_SINK)

#ifndef SLOT_DURATION
#error "Slot duration should be defined"
#endif

#ifndef TIMEOUT
#error "Timeout should be defined"
#endif

#define JITTER_STEP         (0x2) // DWT32 LSB is 4ns, 8ns is therefore 0x2, or 1 << 1
#define MAX_JITTER_MULT     (125) // 1us/8ns

#ifndef NODES_DEPLOYED
#define NODES_DEPLOYED \
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10,\
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,\
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,\
    31, 32, 33, 34, 35, 36,\
    101, 100
#endif // NODES_DEPLOYED

#ifndef WEAVER_N_ORIGINATORS
#define WEAVER_ORIGINATORS_TABLE\
         2,  3,  4,  5,  6,  7,  8,  9, 10,\
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,\
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,\
    31, 32, 33, 34, 35, 36
#define WEAVER_N_ORIGINATORS           1
#define WEAVER_EPOCHS_PER_CYCLE        35
#define WEAVER_APP_START_EPOCH         2
#endif // WEAVER_N_ORIGINATORS

#define PA tsm_prev_action
#define NA tsm_next_action

#ifndef SINK_RADIUS
#define SINK_RADIUS                         4
#endif  // SINK_RADIUS

/* IDLE COUNTER
 * from sink to receive the last peer D slots are required.
 * For its information to receive the sink (2 * (D-2) + 2) are required
 *
 * S T | R | . | T | . | . | T | . | . | T | R
 * . R | T | R | . | T | . | . | T | R |[R | T]
 * .   | R | T | . | . | T | R |[R | T]
 * .   |   | R | T | R |[R | T]
 * D   |   |   | R | T | ...
 */

#define BITMAP_ALL_ONE(TYPE)                ( (TYPE) -1 )
#define IS_SLEEP_BITMAP(BITMAP)             ( BITMAP == BITMAP_ALL_ONE(typeof(BITMAP))  )

#define WEAVER_LOCAL_ACK_SUPPRESSION_INTERVAL(H, C, Y) \
    (2 * (H - 2) + H + 3 * Y -\
     (C + H + 2 * (H - 2)) % (3 * Y))

#ifndef TERMINATION_WAIT_PEER
#define TERMINATION_WAIT_PEER                   (6) // + 3 * GLOBAL_ACK_PERIOD)
#endif // TERMINATION_WAIT_PEER

#ifndef TERMINATION_WAIT_SINK
#define TERMINATION_WAIT_SINK                   (6) // + 3 * GLOBAL_ACK_PERIOD)
#endif // TERMINATION_WAIT_SINK

// (atm) unused parameter. Max epoch is computed based on epoch and slot duration.
// Code kept for possible later introduction
#ifndef WEAVER_MAX_EPOCH_SLOT
#define WEAVER_MAX_EPOCH_SLOT              200
#endif  // WEAVER_MAX_EPOCH_SLOT

#ifndef WEAVER_BOOT_REDUNDANCY
#define WEAVER_BOOT_REDUNDANCY             2
#endif  // WEAVER_BOOT_REDUNDANCY

#ifndef GLOBAL_ACK_PERIOD
#define GLOBAL_ACK_PERIOD                       4
#endif  // GLOBAL_ACK_PERIOD

#ifndef ENABLE_EARLY_PEER_TERMINATION
#define ENABLE_EARLY_PEER_TERMINATION           0
#endif  // ENABLE_EARLY_PEER_TERMINATION

#ifndef WEAVER_PEER_TERMINATION_COUNT
#define WEAVER_PEER_TERMINATION_COUNT      (3 * SINK_RADIUS + 3 * GLOBAL_ACK_PERIOD + 3 * WEAVER_BOOT_REDUNDANCY + 3 + TERMINATION_WAIT_PEER)
#endif  // WEAVER_PEER_TERMINATION_COUNT

#ifndef WEAVER_SINK_TERMINATION_COUNT
#define WEAVER_SINK_TERMINATION_COUNT      (3 * SINK_RADIUS + 3* WEAVER_BOOT_REDUNDANCY)
#endif  // WEAVER_SINK_TERMINATION_COUNT

#ifndef EXTRA_PAYLOAD_LEN
#define EXTRA_PAYLOAD_LEN                       0
#endif  // EXTRA_PAYLOAD_LEN

#define PKT_POOL_LEN                             65
#define IN_BUF_SIZE                             100
#define OUT_BUF_SIZE                            100

#ifndef MAX_RX_CONSECUTIVE_ERRORS
#define MAX_RX_CONSECUTIVE_ERRORS 20
#endif

#if WEAVER_WITH_FS
#define TERMINATION_CONDITION(termination_counter, termination_cap) (true)
#define LAST_FS (last_fs)
#define IS_FS(_nslot) (_nslot - (SINK_RADIUS + 3 * (WEAVER_BOOT_REDUNDANCY-1)) >= 0 && (_nslot - (SINK_RADIUS + 3 * (WEAVER_BOOT_REDUNDANCY-1))) % (3 * GLOBAL_ACK_PERIOD + FS_MACROSLOT) == 0)
#define IS_FS_NEXT() (IS_FS(PA.logic_slot_idx + 1))
#else
#define TERMINATION_CONDITION(termination_counter, termination_cap) (termination_counter < termination_cap)
#define LAST_FS (true)
#endif

#define ADVANCE_GLOBAL_ACK_COUNTER(n) do{global_ack_counter = ((global_ack_counter + n) % (3 * GLOBAL_ACK_PERIOD));} while(0)

#pragma message STRDEF(WEAVER_N_ORIGINATORS)
#pragma message STRDEF(WEAVER_EPOCHS_PER_CYCLE)
#pragma message STRDEF(WEAVER_APP_START_EPOCH)
#pragma message STRDEF(SINK_ID)
#pragma message STRDEF(SINK_RADIUS)
#pragma message STRDEF(WEAVER_BOOT_REDUNDANCY)
#pragma message STRDEF(PKT_POOL_LEN)
#pragma message STRDEF(EXTRA_PAYLOAD_LEN)
#pragma message STRDEF(WEAVER_WITH_FS)
#pragma message STRDEF(GLOBAL_ACK_PERIOD)

// keep information about packet delivered to the app
// and packet required to be spread by weaver.
// App <-> weaver
typedef struct data_log_t {
    uint16_t originator_id;
    uint16_t seqno;
    uint32_t slot_idx;  // slot of reception, only meaningful for receptions
} data_log_t;

static data_log_t in_buf[IN_BUF_SIZE];
static size_t in_pnt = 0;
static data_log_t out_buf[OUT_BUF_SIZE];
static size_t out_pnt = 0;

// return a bitmap where bits flagged are those that turned from 0
// in BITM1 to 1 in BITM2
#define SET_DIFF(BITM1, BITM2)  ((BITM1 ^ BITM2) & BITM2)

typedef struct info_t {
    uint16_t originator_id;
    uint16_t last_heard_originator_id;
    uint8_t  hop_counter;
    uint64_t sink_acked;
    uint16_t epoch;         // data
    uint16_t seqno;
    uint8_t extra_payload[EXTRA_PAYLOAD_LEN];
}
__attribute__ ((packed))
info_t;

// create a linked list of free entries and initialize it
// as a rr table.
static void pkt_pool_init();

static struct peer_rx_ok_return peer_rx_ok();
static inline void pkt_pool_get_mypkt();
static inline void pkt_pool_get_next();
static inline bool pkt_pool_is_empty();
static inline void pkt_pool_remove_nodes(const uint64_t* nodes_to_remove);
static inline uint64_t pkt_pool_get_sender_bitmap();
static inline void print_app_interactions();
static inline int16_t infer_global_ack_counter(const uint32_t slot_idx, const uint8_t node_hop_dist);
static inline void update_termination(
  uint8_t node_hop_correction,
  const int16_t global_ack_counter,
  const int boot_redundancy_counter,
  const struct peer_rx_ok_return rx_updates);

/*---------------------------------------------------------------------------*/
/*                          STATIC VARIABLES                                 */
/*---------------------------------------------------------------------------*/
uint16_t nodes_deployed[MAX_NODES_DEPLOYED] = {NODES_DEPLOYED};
size_t   n_nodes_deployed;
static uint16_t originators_table[] = {WEAVER_ORIGINATORS_TABLE};

static uint8_t buffer[127];     // buffer for TX and RX
static struct  pt pt;           // protothread object
static info_t  node_pkt;
static info_t  rcvd;
static uint64_t node_acked;
static uint8_t  node_dist;
static uint16_t last_heard_originator_id;
static bool     node_has_data;

static int32_t epoch_max_slot;
// termination
static uint16_t termination_counter;
static uint16_t termination_cap;

static weaver_log_t log; // hold log information of a particular slot
static trexd_stats_t trex_stats;

static uint16_t tmp_bitmap_unmapped[MAX_NODES_DEPLOYED];     // buffer for TX and RX

static rr_entry_t pkt_pool_space[PKT_POOL_LEN];
static rr_table_t pkt_pool;

struct peer_rx_ok_return {
    bool new_gack;
    bool new_data;
    bool sleep_rcvd;
    bool buf_emptied;
    bool gacked_data;
};

/* We check that it is
 * As the signal strength cannot be obtained directly we need to use
 * Due to 10*log10((C*2^17)/(N^2) - A) is the signal power in dBm
 * as "4.7.2 Estimating the receive signal power" pg.46
 * In this:
 *   - C is the result of CIR_PWR
 *   - A is 113.77 for PRF 16MHz and is 121.74 for PRF 64MHz
 * Thus if we consider 10*log10((C*2^17)/(N^2) - A) > minDBM we can obtain
 * C/(N^2) > 10 ^ ((minDBM + A)/10 - log10(2^17))
 * We can change this to
 * --> 2^16 * (C/(N^2)) > 2^16 * 10^((minDBM + A)/10 - log10(2^17))
 * For PRF 64 and minDBM=-94dBm this means that 2^16 * (C/(N^2)) > 297.1460793
 */
#define HOPCOUNT_SET_RX_THRESH 297

static inline bool valid_rx_pwr() {
  uint16_t rxPreamCount = (dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXPACC_MASK) >> RX_FINFO_RXPACC_SHIFT;
  uint16_t pacNonsat = dwt_read16bitoffsetreg(DRX_CONF_ID, 0x2C);

  uint16_t maxGrowthCIR = dwt_read16bitoffsetreg(RX_FQUAL_ID, 0x06);

  /* As per "4.7.2 Estimating the receive signal power" pg. 46, RXPACC before being used has to 
   * be adjusted. The adjustment is at Table 18 of the manual and, assuming the standard 8 symbols SFD, is -5.
   * This adjustment has to be applied only when RXPACC and RXPACC_NOSAT are the same
   */
  uint32_t adjusted_rxpacc = rxPreamCount - ((pacNonsat == rxPreamCount)?5:0);

  // Check if the power is high enough to accept the node as the parent
  return (((uint32_t) maxGrowthCIR) << 16)/(adjusted_rxpacc*adjusted_rxpacc) >= HOPCOUNT_SET_RX_THRESH;
}

uint32_t from_minislots_to_logic_slots(uint32_t minislots) {
#if WEAVER_WITH_FS
  uint32_t res = 0;

  /*
   * indexes
   *      minislots | 0| 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|
   *     logic slot |    0   |    1   |    2   |    3   |    4   |    5   |    6   |    7   |                  10                  |
   *
   * hops        0  |    T   |    R   |    R   |    T   |    R   |    R   |    T   |    R   |                Flick                 |
   *             1  |        |    T   |    R   |    R   |    T   |    R   |    R   |    T   |                Flick                 |
   *             2  |        |        |    T   |    R   |    R   |    T   |    R   |    R   |                Flick                 |
   *             3  |        |        |        |    T   |    R   |    R   |    T   |    R   |                Flick                 |
   *             4  |        |        |        |        |    T   |    R   |    R   |    T   |                Flick                 |
   *
   * For this example consider SINK_RADIUS = 4, WEAVER_BOOT_REDUNDANCY = 2
   * 1. Consider that a weaver bootstrap will last SINK_RADIUS + 3*(WEAVER_BOOT_REDUNDANCY-1) logic slots
   * 2. As these logic slots are homogenous in size (TSM_DEFAULT_MINISLOTS_GROUPING), 
   *    the first minislot of the last bootstrap logic slot will be TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + 3*(WEAVER_BOOT_REDUNDANCY-1))
   * 3. We can thus, when considering the first w < TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + 3*(WEAVER_BOOT_REDUNDANCY-1)) slots, just divide
   *    to obtain the current logic slot index
   * 4. We can calculate the logic slot index given from the part before the Flick slots and remove the used minislots
   * (Note that we always have a negative offset of TSM_DEFAULT_MINISLOTS_GROUPING - 1)
   * 5. If we are further than the bootstrap section then we at least scheduled a Flick slot so we can remove the corresponding minislots and add the corresponding logic slots
   * 6. After this weaver becomes regular with a period of TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD + FS_MINISLOT minislots corresponding to 
   *    3 * GLOBAL_ACK_PERIOD + FS_MACROSLOT logic slots. We can then calculate how many whole groups we have and then handle the remainder
   */

  res += MIN(minislots, TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + 3 * (WEAVER_BOOT_REDUNDANCY - 1) )) / TSM_DEFAULT_MINISLOTS_GROUPING;

  if (minislots < TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + 3 * (WEAVER_BOOT_REDUNDANCY-1))) {
    return res;
  }

  minislots -= TSM_DEFAULT_MINISLOTS_GROUPING*(SINK_RADIUS + 3 * (WEAVER_BOOT_REDUNDANCY-1));
  res += FS_MACROSLOT;

  if (minislots < FS_MINISLOT) {
    return res;
  }

  minislots -= FS_MINISLOT;

  uint32_t cycles = minislots/(TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD + FS_MINISLOT);

  res += cycles*(3 * GLOBAL_ACK_PERIOD + FS_MACROSLOT);
  minislots -= cycles * (TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD + FS_MINISLOT);

  res += MIN(minislots, TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD) / TSM_DEFAULT_MINISLOTS_GROUPING;

  if (minislots < TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD) {
    return res;
  }

  minislots -= TSM_DEFAULT_MINISLOTS_GROUPING * 3 * GLOBAL_ACK_PERIOD;
  res += FS_MACROSLOT;

  if (minislots < FS_MINISLOT) {
    return res;
  } else {
    ERR("This should not be possible");
    return 0;
  }
#else
  /*
   * In normal weaver all the slots are homogenous so we can just divide by the size in minislots of a normal logic slot
   */
  return minislots / TSM_DEFAULT_MINISLOTS_GROUPING;
#endif
}

/*---------------------------------------------------------------------------*/
// moved here for visibility
static int16_t global_ack_counter;
/*---------------------------------------------------------------------------*/
static char sink_thread() {
    static uint16_t epoch;

    static uint16_t offset_slots;
#if WEAVER_WITH_FS
    static bool     last_fs;
#else
    static uint16_t ntx_slot;
#endif

    static int boot_redundancy_counter;

    PT_BEGIN(&pt);

    node_dist = 0;
    epoch = 0;
    while (1) {
        out_pnt = 0; in_pnt = 0;
        memset(in_buf, 0, sizeof(in_buf));
        memset(out_buf, 0, sizeof(out_buf));
        STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
        if (epoch < 20) {
          // Do not consider statetime in the first epochs but wait some time so that there is enough stable data to estimate the ratio between radio and cpu clock
          STATETIME_MONITOR(dw1000_statetime_stop());
        }
        weaver_log_init();
        epoch ++;                       // start from epoch 1
        node_acked = 0x0;               // forget previous acks
        global_ack_counter = 0;
        termination_counter = 0;

        offset_slots = 0;
#if WEAVER_WITH_FS
        last_fs = true;
#endif

        // Send GlossyTxOnly sync flood
        node_pkt.epoch         = epoch;
        node_pkt.originator_id = SINK_ID_CONSTANT;
        node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
        node_pkt.hop_counter   = node_dist;
        node_pkt.sink_acked    = node_acked;

        termination_cap     = WEAVER_SINK_TERMINATION_COUNT;
        global_ack_counter  = infer_global_ack_counter(PA.logic_slot_idx + 1 - offset_slots, node_dist);
        boot_redundancy_counter = WEAVER_BOOT_REDUNDANCY;

        while (PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS) {
            node_pkt.epoch         = epoch;
            node_pkt.originator_id = SINK_ID_CONSTANT;
            node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
            node_pkt.hop_counter   = node_dist;
            node_pkt.sink_acked    = node_acked;

#if WEAVER_WITH_FS
            if (IS_FS_NEXT()) {
                NA.max_fs_flood_duration = MAX_LATENCY_FS;
                NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;

                TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);

                offset_slots += FS_MACROSLOT;

                if (PA.status == TREX_FS_EMPTY) {
                  // If we did not receive nor re-propagate the flood

                  last_fs = false;
                  break;
                } else if (PA.status == TREX_FS_ERROR){
                  ERR("Unexpected result from Flick");
                } else {
                  // Received and repropagated the flood
                }
            }
#endif

            if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS)) {
              // Exit if due the previous operation we are no longer allowed to be in the loop
              break;
            }

            while (((PA.logic_slot_idx - offset_slots + 1) % (WEAVER_NRX + WEAVER_NTX)) < WEAVER_NTX) {
                if ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) < 0) {
                    WARN("Negative index in modulo arithmetic for slot indexes");
                }

                memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

                ADVANCE_GLOBAL_ACK_COUNTER(1);
                termination_counter += 1;

                log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                    .node_dist = node_dist,
                    .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                    .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                weaver_log_append(&log);

#if WEAVER_WITH_FS
                if (IS_FS_NEXT()) {
                    NA.max_fs_flood_duration = MAX_LATENCY_FS;
                    NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                    TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);

                    offset_slots += FS_MACROSLOT;

                    if (PA.status == TREX_FS_EMPTY) {
                      // If we did not receive nor re-propagate the flood

                      last_fs = false;
                      break;
                    } else if (PA.status == TREX_FS_ERROR){
                      ERR("Unexpected result from Flick");
                    } else {
                      // Received and repropagated the flood
                    }
                }
#endif

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS)) {
                  // Exit if due the previous operation we are no longer allowed to be in the loop
                  break;
                }
            }

            if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS)) {
              // Exit if due the previous operation we are no longer allowed to be in the loop
              break;
            }

            while (((PA.logic_slot_idx - offset_slots + 1) % (WEAVER_NRX + WEAVER_NTX)) >= WEAVER_NTX) {
                if ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) < 0) {
                    WARN("Negative index in modulo arithmetic for slot indexes");
                }

                TSM_RX_SLOT(&pt, buffer);

                if (PA.status == TREX_RX_SUCCESS) {
                    memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));

                    if (IS_SLEEP_BITMAP(rcvd.sink_acked)) {
                        // something very wrong happened...
                        // Notify the occurrence, but simply ignore the packet.
                        ERR("The sink received a sleep command.");
                        termination_counter += 1;
                        continue;
                    }

                    if (rcvd.originator_id != SINK_ID_CONSTANT &&
                        !is_node_acked(node_acked, rcvd.originator_id)) {

                        node_acked = ack_node(node_acked, rcvd.originator_id);
                        termination_counter = 0;
                        termination_cap = 3 * GLOBAL_ACK_PERIOD - global_ack_counter + 3 * SINK_RADIUS + 3 * boot_redundancy_counter + TERMINATION_WAIT_SINK;

                        in_buf[in_pnt] = (data_log_t) {.originator_id = rcvd.originator_id,
                            .seqno = rcvd.seqno, .slot_idx = PA.minislot_idx};
                        in_pnt += in_pnt < IN_BUF_SIZE ? 1 : 0;
                    }
                    else {
                        termination_counter += 1;
                    }

                    log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                    weaver_log_append(&log);
                }
                else if (PA.status == TREX_RX_ERROR) {  // NOTE: it's unclear when TREX_RX_MALFORMED occurs
                    termination_counter = 0;
                    termination_cap = 3 * GLOBAL_ACK_PERIOD - global_ack_counter + 3 * SINK_RADIUS + 3 * boot_redundancy_counter + TERMINATION_WAIT_SINK;
                }
                else if (PA.status == TREX_RX_TIMEOUT) {
                    termination_counter += 1;
                }

                ADVANCE_GLOBAL_ACK_COUNTER(1);

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS)) {
                  // Exit if due the previous operation we are no longer allowed to be in the loop
                  break;
                }

#if WEAVER_WITH_FS
                if (IS_FS_NEXT()) {
                    NA.max_fs_flood_duration = MAX_LATENCY_FS;
                    NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                    TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);

                    offset_slots += FS_MACROSLOT;

                    if (PA.status == TREX_FS_EMPTY) {
                      // If we did not receive nor re-propagate the flood

                      last_fs = false;
                      break;
                    } else if (PA.status == TREX_FS_ERROR){
                      ERR("Unexpected result from termination Flick %hu", PA.status);
                    } else {
                      // Received and repropagated the flood
                    }
                }
#endif

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && LAST_FS)) {
                  // Exit if due the previous operation we are no longer allowed to be in the loop
                  break;
                }
            }

            boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0: boot_redundancy_counter-1;
        }

        if (PA.minislot_idx >= epoch_max_slot) {
          WARN("Exit e %hu (max slot)", epoch);
        }

#if WEAVER_WITH_FS
        if (!LAST_FS) {
          PRINT("Exit e %hu (negative flick)", epoch);
        }
#else
        if (! (TERMINATION_CONDITION(termination_counter, termination_cap))) {
          WARN("Exit e %hu (termination cap)", epoch);
        }
#endif

        // TX sleep command in a Glossy-like manner if time allows.
        // Prepare a packet with all bitmap flagged, to signal the network
        // is going to sleep.
        node_pkt.originator_id = SINK_ID_CONSTANT;

#if !WEAVER_WITH_FS
        node_pkt.epoch         = epoch;
        node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
        node_pkt.hop_counter   = node_dist;
        node_pkt.sink_acked    = BITMAP_ALL_ONE(typeof(node_pkt.sink_acked));
        memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

        ntx_slot = 0;
        while (PA.minislot_idx < epoch_max_slot && ntx_slot < WEAVER_SLEEP_NTX) {
            TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

            log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                .node_dist = node_dist,
                .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                .acked = node_pkt.sink_acked, .buffer = pkt_pool_get_sender_bitmap()};
            weaver_log_append(&log);

            ntx_slot ++;
        }
#endif

        logging_context = epoch;
        printf("E %"PRIu32", NSLOTS %" PRIu32"\n", logging_context, PA.minislot_idx < 0 ? 0 : PA.minislot_idx + 1);
        print_acked(node_acked);
        print_app_interactions();
        TREX_STATS_PRINT();
        STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());
        WEAVER_LOG_PRINT();

        TSM_RESTART(&pt, PERIOD_SINK);
    }
    PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
#define WEAVER_MISSED_BOOTSTRAP_BEFORE_SCAN    3
static char peer_thread() {
    static uint16_t epoch = 0;
    static uint16_t ntx_slot;
    static bool silent_tx;
    static bool is_originator;
    static bool is_sync = false;
    static int  n_missed_bootstrap = 0;
    static bool is_bootstrapped = false;
    static int  boot_redundancy_counter;
    static struct peer_rx_ok_return rx_updates; // received something new in the very current slot
    static bool node_pkt_bootstrapped;
    static bool new_gack_last_tx;               // flag for global ACKs reception
    static bool i_tx;                           // force TX in next iteration
    static uint16_t seqno;
    static bool leak;                           // set to true when receiving a packet from a previous epoch in the scanning phase
    static bool must_sleep;                     // set to true when the node receives a sleep command from the sink
    static bool resend_gack;                    // heard a packet that was already ACKed by sink; trigger transmission

    static uint16_t offset_slots;
    static uint16_t consecutive_rx_errors;
#if WEAVER_WITH_FS
    static bool     last_fs;
#endif

    PT_BEGIN(&pt);

    is_originator = false;
    seqno = 0;
    leak = false;
    while (1) { // Epoch while
        out_pnt = 0; in_pnt = 0;
        memset(in_buf, 0, sizeof(in_buf));
        memset(out_buf, 0, sizeof(out_buf));
        STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
        if (epoch < 20) {
          STATETIME_MONITOR(dw1000_statetime_stop());
        }
        weaver_log_init();
        pkt_pool_init();                     // clean and init pkt_pool
        node_acked = 0x0;
        node_dist = UINT8_MAX;
        node_has_data = false;
        last_heard_originator_id = SINK_ID_CONSTANT;
        boot_redundancy_counter = WEAVER_BOOT_REDUNDANCY;
        must_sleep = false;

        offset_slots = 0;
        consecutive_rx_errors = 0;
#if WEAVER_WITH_FS
        last_fs = true;
#endif

        if (is_originator && WEAVER_BOOT_REDUNDANCY > 0) {
            boot_redundancy_counter += 1;
            node_pkt_bootstrapped = false;
        }

        if (is_originator) {
            seqno ++;
            node_pkt.seqno = seqno;
            node_pkt.epoch = 0;                 // unknown epoch atm
            node_pkt.originator_id = node_id;
            node_pkt.last_heard_originator_id = node_id;
            node_pkt.hop_counter = node_dist;   // unkown hop distance atm
            node_pkt.sink_acked  = node_acked;
            int k;
            for(k=0; k<EXTRA_PAYLOAD_LEN; k++) node_pkt.extra_payload[k] = node_id;
            rr_table_add(&pkt_pool, node_id, (uint8_t*) &node_pkt, sizeof(info_t));
            node_has_data = true;

            out_buf[out_pnt] = (data_log_t) {.originator_id = node_id,
                .seqno = seqno, .slot_idx = 0};
            out_pnt += out_pnt < OUT_BUF_SIZE ? 1 : 0;
        }

        // if we miss X consecutive bootstraps, we start scanning again
        /* Bootstrap: In the very first epoch the node will continuously scan
         * the network until succesfully receive a packet. Thereafter the node
         * is "synced" and will bootstrap using simple rx slots bounded by the
         * termination counter logic. If some X number of consecutive bootstraps
         * occurs without receiving a packet, the node is "de-synced" and falls
         * back scanning undeterminately.
         *
         * PS: a node is bootstrapped only if it managed to bootstrap in
         * the current epoch.
         */
        termination_cap = WEAVER_PEER_TERMINATION_COUNT + 3;   // termination cap for the bootstrap phase
        termination_counter = 0;
        is_bootstrapped = false;
        NA.rx_guard_time = (1000 * UUS_TO_DWT_TIME_32); // start some time earlier than the sink
        while (1) { // Bootstrap while
#if WEAVER_WITH_FS
            /* If while we are looking for the bootstrap we reach the slot at which we are supposed to 
             * use Flick and we think we are "synchronized enough" (i.e. we didn't lose too many consecutive bootstrap) 
             * schedule a Flick slot */
            if ((epoch > WEAVER_APP_START_EPOCH) && is_sync && IS_FS_NEXT()) {
              if (node_has_data) {
                  TSM_TX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);

                  // Should always continue
                  last_fs = true;

                  /*
                   * After completing the Flick slot, send the packet in the hope that a node will see it and respond to it with a LACK,
                   * so that the node can synchronize. This can be helpful when there is a single originator as otherwise the node won't
                   * ever see other traffic, as is the sole originators, and thus won't ever be to synchronize and send its packet
                   * NOTE: Here we do not take into account boot_redundancy_counter as if we re-synchronize after we still want to send the data again
                   */
                  if (is_originator && WEAVER_BOOT_REDUNDANCY > 0) {
                      PRINT("Transmit while in bootstrap scan, epoch %hu", epoch);

                      pkt_pool_get_mypkt();

                      node_pkt.last_heard_originator_id = last_heard_originator_id;
                      node_pkt.hop_counter = node_dist;
                      node_pkt.sink_acked  = node_acked;
                      node_pkt.epoch = epoch;
                      memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t));

                      TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

                      // prepare log, keeping the originator_id that was set when preparing the packet
                      log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                          .node_dist = node_dist,
                          .originator_id = node_id, // keep the same originator_id, since it was set just before TX
                          .lhs = last_heard_originator_id,
                          .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};

                      weaver_log_append(&log);
                  }
              } else {
                  NA.max_fs_flood_duration = MAX_LATENCY_FS;
                  NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                  TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);

                  if (PA.status == TREX_FS_EMPTY) {
                      // Should stop and go to the next epoch
                      PRINT("Nothing responded to Flick, restart");
                      last_fs = false;
                  } else if (PA.status == TREX_FS_ERROR) {
                      ERR("Unexpected result from sync Flick %hu", PA.status);
                      last_fs = false;
                  } else {
                      last_fs = true;
                  }
              }

              offset_slots += FS_MACROSLOT;
            }
#else
            /*
             * If, while waiting to receive the bootstrap we reach the slot after which the bootstrap should have propagated throughout the network
             * and we are "synchronized enough" (i.e. we did not lose too many consecutive bootstraps) and this node is an originator
             * send the packet in the hope that a node will respond with a LACK so that this node can synchronize. 
             * This can be helpful when there is a single originator as otherwise the node won't ever see other
             *  traffic, as is the sole originators, and thus won't ever be to synchronize and send its packet
             * NOTE: Here we do not take into account boot_redundancy_counter as if we re-synchronize after we still want to send the data again
             */
            if ((epoch > WEAVER_APP_START_EPOCH) && 
                is_sync &&
                (SINK_RADIUS + 3 * WEAVER_BOOT_REDUNDANCY - PA.logic_slot_idx == 1) &&
                node_has_data &&
                is_originator &&
                WEAVER_BOOT_REDUNDANCY > 0) {

                PRINT("Transmit while in bootstrap scan, epoch %hu", epoch);
                // trasmit node's pkt in the B+1 TR slot during the bootstrapping phase
                pkt_pool_get_mypkt();

                node_pkt.last_heard_originator_id = last_heard_originator_id;
                node_pkt.hop_counter = node_dist;
                node_pkt.sink_acked  = node_acked;
                node_pkt.epoch = epoch;
                memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t));

                TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

                // prepare log, keeping the originator_id that was set when preparing the packet
                log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                    .node_dist = node_dist,
                    .originator_id = node_id, // keep the same originator_id, since it was set just before TX
                    .lhs = last_heard_originator_id,
                    .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};

                weaver_log_append(&log);
            }
#endif

            /* If are synchronized enough (i.e. we did not lose too many consecutive bootstrap)
             * use a RX slot (which consumes less energy but requires synchronization)
             * otherwise use a SCAN slot (which consumes more power but does not require synchronization)
             */
            if (is_sync) {
                TSM_RX_SLOT(&pt, buffer);
                termination_counter += 1;
            }
            else {
                TSM_SCAN(&pt, buffer);
            }

            if (PA.status == TREX_RX_SUCCESS) {
                // Check if the power is high enough to accept the node as the parent
                if (valid_rx_pwr()) {
                  memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                  if (rcvd.originator_id != SINK_ID_CONSTANT) {
                    memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
                  }

                  if (rcvd.hop_counter == UINT8_MAX) {
                    // If this was a packet sent by a node while trying to receive a bootstrap ignore it
                    // (This node is too looking for a bootstrap)
                    continue;
                  }

                  if (rcvd.epoch > epoch) {
                      epoch = rcvd.epoch;
                  }
                  else {
                      is_bootstrapped = false;
                      leak = true;
                      continue;
                  }

                  uint64_t nodes_to_remove = SET_DIFF(node_acked, rcvd.sink_acked);
                  if (nodes_to_remove > 0) {
                      pkt_pool_remove_nodes(&nodes_to_remove);
                  }
                  node_acked |= rcvd.sink_acked;
                  node_dist = rcvd.hop_counter + 1;

                  log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                      .node_dist = node_dist,
                      .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                      .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                  weaver_log_append(&log);

                  is_bootstrapped = true;
                  n_missed_bootstrap = 0;

                  // request TSM to resynchronise using the received packet regardless of us accepting the parent or not
                  NA.accept_sync = 1;

                  break;
                } else {
                  is_bootstrapped = false;
                }
            }
            else {
                is_bootstrapped = false;
            }

            if (termination_counter >= termination_cap) {
              epoch +=1;
              n_missed_bootstrap ++;
              // is_bootstrapped = false; // probably not needed, should be already false
              break;
            }
        }
        // Used in next epoch
        is_sync = n_missed_bootstrap >= WEAVER_MISSED_BOOTSTRAP_BEFORE_SCAN ? false : true;

        if (!is_originator && !pkt_pool_is_empty()) {
          ERR("The packet pool is not empty! (e %hu)", epoch);
        }

        /* If we skipped the synchronization phase print a warning.
         * Note that as is_bootstrapped should be false we will not enter the
         * following loop that constitutes the main part of the protocol
         */
        if (PA.logic_slot_idx - node_dist >= WEAVER_BOOT_REDUNDANCY ) {
          WARN("Skipped synch phase");
        }

        /*--------------------------------------------------------------------*/

        // Reset termination cap and counter as we got at least one reception of the sync
        // NOTE: Old ``optimized'' bootstrap termination cap 3 * SINK_RADIUS + 3 * GLOBAL_ACK_PERIOD + 3 + TERMINATION_WAIT_PEER;
        termination_cap = WEAVER_PEER_TERMINATION_COUNT;
        termination_counter = 0;
        silent_tx = false;
        i_tx = false;

        /* If the bootstrap failed do not run infer_global_ack_counter as it is not needed
         * and the result could create warnings as the data passed to the function might not make
         * sense */
        if (is_bootstrapped) {
          global_ack_counter = infer_global_ack_counter(PA.logic_slot_idx + NA.progress_logic_slots - offset_slots, node_dist);
        }

        while (is_bootstrapped &&
               !must_sleep &&
               PA.minislot_idx < epoch_max_slot &&
               TERMINATION_CONDITION(termination_counter, termination_cap) &&
               consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS &&
               LAST_FS
            ) {
            resend_gack = false;

#if WEAVER_WITH_FS
            if (IS_FS_NEXT()) {
                if (rr_table_check_any_negative_deadline(&pkt_pool, PA.logic_slot_idx + 1 + FS_MACROSLOT)) {
                  TSM_TX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                } else {
                  NA.max_fs_flood_duration = MAX_LATENCY_FS;
                  NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                  TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                }

                offset_slots += FS_MACROSLOT;

                if (PA.status == TREX_FS_EMPTY) {
                  // If we did not receive nor re-propagate the flood

                  last_fs = false;
                  break;
                } else if (PA.status == TREX_FS_ERROR){
                  ERR("Unexpected result from termination Flick %hu", PA.status);
                } else {
                  // Received and repropagated the flood
                }
            }
#endif

            if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                // Exit if due the previous operation we should no longer be in the loop
                break;
            }

            // NOTE: When cleaning code put WEAVER_NTX to always 1/remove WEAVER_NTX
            while ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) < WEAVER_NTX) {
                if ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) < 0) {
                    WARN("Negative index in modulo arithmetic for slot indexes");
                }

                if (!silent_tx) {
                    if (node_has_data) {
                        node_pkt.last_heard_originator_id = last_heard_originator_id;
                        node_pkt.hop_counter = node_dist;
                        node_pkt.sink_acked  = node_acked;
                        node_pkt.epoch = epoch;
                        memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t));

                        log.originator_id = node_pkt.originator_id;
                    }
                    else {
                        info_t tmp = {.epoch = epoch, .originator_id = SINK_ID_CONSTANT,
                            .last_heard_originator_id = last_heard_originator_id,
                            .sink_acked=node_acked,
                            .hop_counter = node_dist};
                        memcpy(buffer+TSM_HDR_LEN, &tmp, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

                        log.originator_id = tmp.originator_id;
                    }

                    NA.tx_delay = MAX_JITTER_MULT ? ((random_rand() % (MAX_JITTER_MULT+1))*JITTER_STEP) : 0;

                    if (node_has_data) { TSM_TX_SLOT(&pt, buffer, sizeof(info_t)); }
                    else { TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN); }

                    termination_counter += 1;
                    boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0 : boot_redundancy_counter - 1;
                    new_gack_last_tx = false;

                    // prepare log, keeping the originator_id that was set when preparing the packet
                    log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = log.originator_id, // keep the same originator_id, since it was set just before TX
                        .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};

                    weaver_log_append(&log);

                    last_heard_originator_id = SINK_ID_CONSTANT;
                }
                else {
                    silent_tx = false;
                    last_heard_originator_id = SINK_ID_CONSTANT;

                    TSM_RX_SLOT(&pt, buffer);

                    if (PA.status == TREX_RX_SUCCESS) {
                        consecutive_rx_errors = 0;

                        memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                        if (rcvd.originator_id != SINK_ID_CONSTANT) {
                          memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
                        }

                        rx_updates = peer_rx_ok();
                        must_sleep |= rx_updates.sleep_rcvd;
                        new_gack_last_tx |= rx_updates.new_gack;
                        resend_gack |= rx_updates.gacked_data;

                        log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                            .node_dist = node_dist,
                            .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                            .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                        weaver_log_append(&log);

                        update_termination(node_dist, global_ack_counter, boot_redundancy_counter, rx_updates);
                    }
                    /* We check that it is
                     * 1. An error
                     * 2. It is an error due PHE (PHY header error), FCE (CRC Error), RFSL (Reed Solomon Error)
                     * 3. It didn't reach the point at which the CIR is obtained or the estimated RX signal strength is > -95dBm
                     * if it is one of this we intrepret it as an error and reset the termination counter
                     * otherwise consider as if we received nothing (probably just some noise)
                     */
                    else if ( PA.status == TREX_RX_ERROR && 
                             (PA.radio_status & (SYS_STATUS_RXPHE | SYS_STATUS_RXFCE | SYS_STATUS_RXRFSL)) &&
                             (!(PA.radio_status & (SYS_STATUS_LDEDONE)) || valid_rx_pwr())
                            ) {  // NOTE: it's unclear when TREX_RX_MALFORMED occurs
                        termination_counter = 0;
                        consecutive_rx_errors += 1;
                    }
                    else if (PA.status == TREX_RX_TIMEOUT) {
                        termination_counter += 1;
                    }
                }

                ADVANCE_GLOBAL_ACK_COUNTER(1);

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                    // Exit if due the previous operation we should no longer be in the loop
                    break;
                }

#if WEAVER_WITH_FS
                if (IS_FS_NEXT()) {
                    if (rr_table_check_any_negative_deadline(&pkt_pool, PA.logic_slot_idx + 1 + FS_MACROSLOT)) {
                      TSM_TX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                    } else {
                      NA.max_fs_flood_duration = MAX_LATENCY_FS;
                      NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                      TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                    }

                    offset_slots += FS_MACROSLOT;

                    if (PA.status == TREX_FS_EMPTY) {
                      // If we did not receive nor re-propagate the flood

                      last_fs = false;
                      break;
                    } else if (PA.status == TREX_FS_ERROR){
                      ERR("Unexpected result from termination Flick %hu", PA.status);
                    } else {
                      // Received and repropagated the flood
                    }
                }
#endif

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                    // Exit if due the previous operation we should no longer be in the loop
                    break;
                }
            }

            if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                // Exit if due the previous operation we should no longer be in the loop
                break;
            }

            // RX slots of the schedule;
            // now lhs is either SINK_ID_CONSTANT or some originator_id received in the overhearing slot
            while ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) >= WEAVER_NTX) {
                if ((PA.logic_slot_idx - offset_slots - node_dist + 1) % (WEAVER_NRX + WEAVER_NTX) < 0) {
                    WARN("Negative index in modulo arithmetic for slot indexes");
                }

                silent_tx = false;
                TSM_RX_SLOT(&pt, buffer);

                if (PA.status == TREX_RX_SUCCESS) {
                    consecutive_rx_errors = 0;

                    memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                    if (rcvd.originator_id != SINK_ID_CONSTANT) {
                      memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
                    }

                    rx_updates = peer_rx_ok();
                    must_sleep |= rx_updates.sleep_rcvd;
                    new_gack_last_tx |= rx_updates.new_gack;
                    resend_gack |= rx_updates.gacked_data;

                    log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                    weaver_log_append(&log);

                    update_termination(node_dist, global_ack_counter, boot_redundancy_counter, rx_updates);

                    if (rcvd.hop_counter == UINT8_MAX) {
                      // Some node sent a packet while not being bootstrapped, do not consider it for updating the counters
                    } else if ((rcvd.hop_counter + 1 < node_dist) && valid_rx_pwr()) {
                        node_dist = rcvd.hop_counter + 1;
                        // recompute the local global ack counter based
                        // on the new node distance from the sink
                        global_ack_counter = infer_global_ack_counter(PA.logic_slot_idx + 1 - offset_slots, node_dist);
                        break;
                    }
                }
                /* We check that it is
                 * 1. An error
                 * 2. It is an error due PHE (PHY header error), FCE (CRC Error), RFSL (Reed Solomon Error)
                 * 3. It didn't reach the point at which the CIR is obtained or the estimated RX signal strength is > -95dBm
                 * if it is one of this we intrepret it as an error and reset the termination counter
                 * otherwise consider as if we received nothing (probably just some noise)
                 */
                else if ( PA.status == TREX_RX_ERROR && 
                         (PA.radio_status & (SYS_STATUS_RXPHE | SYS_STATUS_RXFCE | SYS_STATUS_RXRFSL)) &&
                         (!(PA.radio_status & (SYS_STATUS_LDEDONE)) || valid_rx_pwr())
                        ) {
                    termination_counter = 0;
                    consecutive_rx_errors += 1;
                }
                else if (PA.status == TREX_RX_TIMEOUT) {
                    termination_counter += 1;
                }

                ADVANCE_GLOBAL_ACK_COUNTER(1);

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                    // Exit if due the previous operation we should no longer be in the loop
                    break;
                }

#if WEAVER_WITH_FS
                if (IS_FS_NEXT()) {
                    if (rr_table_check_any_negative_deadline(&pkt_pool, PA.logic_slot_idx + 1 + FS_MACROSLOT)) {
                      TSM_TX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                    } else {
                      NA.max_fs_flood_duration = MAX_LATENCY_FS;
                      NA.rx_guard_time = ( 2 + SNIFF_FS_OFF_TIME ) * UUS_TO_DWT_TIME_32;
                      TSM_RX_FS_SLOT(&pt, FS_MACROSLOT, FS_MINISLOT);
                    }

                    offset_slots += FS_MACROSLOT;

                    if (PA.status == TREX_FS_EMPTY) {
                      // If we did not receive nor re-propagate the flood

                      last_fs = false;
                      break;
                    } else if (PA.status == TREX_FS_ERROR) {
                      ERR("Unexpected result from termination Flick %hu", PA.status);
                    } else {
                      // Received and repropagated the flood
                    }
                }
#endif

                if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                    // Exit if due the previous operation we should no longer be in the loop
                    break;
                }
            }

            if (!(PA.minislot_idx < epoch_max_slot && TERMINATION_CONDITION(termination_counter, termination_cap) && consecutive_rx_errors < MAX_RX_CONSECUTIVE_ERRORS && LAST_FS)) {
                // Exit if due the previous operation we should no longer be in the loop
                break;
            }

            if (is_originator &&
                (!node_pkt_bootstrapped) &&
                is_node_acked(node_acked, node_id)) {
                boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0 : boot_redundancy_counter - 1; // just to avoid underflow
                node_pkt_bootstrapped = true;
            }

            if (! (global_ack_counter == 0 && PA.status == TREX_RX_ERROR)) {
                // update pkt_pool counters. If current slot > expected slot then
                // their counter is set to -1 and will be transmitted again in the future
                rr_table_update_deadlines(&pkt_pool, PA.logic_slot_idx);
                pkt_pool_get_next();
            }

            // Boolean to force tranmission of ACK-only messages
            i_tx = (global_ack_counter == 0) && (new_gack_last_tx);

            // Decide if the next TR slot should be silent (account for i_tx)
            if ((!resend_gack && !i_tx && !node_has_data && boot_redundancy_counter <= 0) 
                || ((global_ack_counter == 0) && !new_gack_last_tx && (PA.status == TREX_RX_ERROR || PA.status == TREX_RX_TIMEOUT))
            )  {
              silent_tx = true;
            }
        }

        if (PA.minislot_idx >= epoch_max_slot) {
          WARN("Exited epoch %hu due reaching max slot", epoch);
        }

        if (consecutive_rx_errors >= MAX_RX_CONSECUTIVE_ERRORS) {
          WARN("Exited epoch %hu due max rx consecutive errors", epoch);
        }

#if WEAVER_WITH_FS
        if (!LAST_FS) {
          PRINT("Exited epoch %hu due negative Flick", epoch);
        }
#else
        if (! TERMINATION_CONDITION(termination_counter, termination_cap)) {
          WARN("Exited epoch %hu due termination cap", epoch);
        }
#endif

        // TX sleep command in a Glossy-like manner if time allows.
        // Prepare a packet with all bitmap flagged, to signal the network
        // is going to sleep.
        node_pkt.originator_id = SINK_ID_CONSTANT;

#if !WEAVER_WITH_FS
        node_pkt.epoch         = epoch;
        node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
        node_pkt.hop_counter   = node_dist;
        node_pkt.sink_acked    = BITMAP_ALL_ONE(typeof(node_pkt.sink_acked));
        memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t));

        ntx_slot = 0;
        while (must_sleep && PA.minislot_idx < epoch_max_slot && ntx_slot < WEAVER_SLEEP_NTX) {
            TSM_TX_SLOT(&pt, buffer, sizeof(info_t));

            log = (weaver_log_t) {.idx = PA.logic_slot_idx, .slot_status = PA.status,
                .node_dist = node_dist,
                .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                .acked = node_pkt.sink_acked, .buffer = pkt_pool_get_sender_bitmap()};
            weaver_log_append(&log);

            ntx_slot ++;
        }
#endif

        logging_context = epoch;
        if (is_originator) {
            printf("E %lu, IS_ORIG\n", logging_context);
        }
        printf("E %"PRIu32", NSLOTS %"PRIu32"\n", logging_context, PA.minislot_idx< 0 ? 0 : PA.minislot_idx+ 1);
        PRINTF("BOOT %lu, B %d, N %d, L %d\n", logging_context, is_bootstrapped, n_missed_bootstrap, leak);
        leak = false;
        print_acked(node_acked);
        static char label[] = "BUF";
        print_bitmap(label, 4, pkt_pool_get_sender_bitmap());
        print_app_interactions();
        TREX_STATS_PRINT();
        STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());
        WEAVER_LOG_PRINT();

        // APP-code:
        // Check if the node is an originator in the next epoch.
        // start generating data from a given epoch
        is_originator = false;
        if (epoch >= WEAVER_APP_START_EPOCH) {
            if (WEAVER_N_ORIGINATORS == 0) {
                is_originator = false;
            }
            else {
                int cur_idx;  // determine the offset wrt the epoch considered
                int i;
                cur_idx = ((epoch - WEAVER_APP_START_EPOCH) % WEAVER_EPOCHS_PER_CYCLE) * WEAVER_N_ORIGINATORS;
                for (i = 0; i < WEAVER_N_ORIGINATORS; i++) {
                    if (node_id == originators_table[cur_idx + i]) {
                        is_originator = true;
                        break;
                    }
                }
            }
        }

        TSM_RESTART(&pt, PERIOD_PEER);

    }
    PT_END(&pt);
}

/*---------------------------------------------------------------------------*/
///////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------*/
PROCESS(weaver_test, "Weaver Test");
AUTOSTART_PROCESSES(&weaver_test);
PROCESS_THREAD(weaver_test, ev, data)
{
    PROCESS_BEGIN();
    static struct etimer et;
    static size_t tmp_i;
    static uint64_t tmp_bitmap;
    static uint32_t max_jitter_4ns;
    static uint32_t rx_timeout;
    static uint32_t slot_duration;

    deployment_set_node_id_ieee_addr();
    deployment_print_id_info();
    tsm_init();
    map_nodes();

    printf("Starting Weaver\n");
    logging_context = 0;
    tmp_i = 0;
    for (; tmp_i < n_nodes_deployed; tmp_i++) {
        tmp_bitmap |= ((uint64_t) 1 << tmp_i);
    }

    do {
        char prefix[3] = "BO";
        print_bitmap(prefix, 3, tmp_bitmap); // Bitmap Order: list node ids accordingly to their position in the bitmap
    } while (0);
    do {
        char prefix[3] = "ME";
        tmp_bitmap = 0x0;
        tmp_bitmap = ack_node(tmp_bitmap, node_id);

        if (tmp_bitmap != 0x0) {
          print_bitmap(prefix, 3, tmp_bitmap); // ERROR CHECK: print my node id using the bitmap
        }
    } while (0);

    max_jitter_4ns = 0;
#if MAX_JITTER_MULT > 0
    max_jitter_4ns = JITTER_STEP * MAX_JITTER_MULT;
#endif

    slot_duration = SLOT_DURATION;
    rx_timeout = TIMEOUT;

    epoch_max_slot = (PERIOD_SINK/SLOT_DURATION) - TSM_DEFAULT_MINISLOTS_GROUPING - FS_MINISLOT;

    // NOTE: This factor could probably be improved with some testing
    epoch_max_slot = (epoch_max_slot * 2) / 3;

    tsm_set_default_preambleto(32*PRE_SYM_PRF64_TO_DWT_TIME_32 + max_jitter_4ns);

    printf("RXTO %"PRIu32"\n", rx_timeout);
    printf("SLOT_DURATION %"PRIu32"\n", slot_duration);
    printf("MAX_SLOT_IDX %"PRIu32"\n", epoch_max_slot);
    printf("MAX_FS_FLOOD_DURATION %"PRIu32"\n", MAX_LATENCY_FS);
    printf("PERIOD EPOCH %"PRIu32"\n", PERIOD_SINK);

    printf("Initial nslots %"PRIu32"\n", (PERIOD_SINK/SLOT_DURATION));
    printf("Period sink %"PRIu32"\n", PERIOD_SINK);
    printf("Slot duration %"PRIu32"\n", SLOT_DURATION);

    if (node_id == SINK_ID) {
        PRINTF("IS_SINK\n");
        etimer_set(&et, CLOCK_SECOND * 10);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));
        tsm_minislot_start(slot_duration, rx_timeout, (tsm_slot_cb)sink_thread, &from_minislots_to_logic_slots);
    }
    else {
        etimer_set(&et, CLOCK_SECOND * 2);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));
        tsm_minislot_start(slot_duration, rx_timeout, (tsm_slot_cb)peer_thread, &from_minislots_to_logic_slots);
    }

    PROCESS_END();
}


static void
pkt_pool_init()
{
    rr_entry_t* prev = NULL;
    rr_entry_t* tmp = pkt_pool_space;
    for (; tmp < pkt_pool_space + PKT_POOL_LEN; tmp++) {
        if (prev != NULL) {
            prev->next = tmp;
        }
        tmp->next = NULL;
        tmp->originator_id = 0;
        tmp->data_len = 0;
        tmp->deadline = -1;
        memset(tmp->data, 0, RR_TABLE_MAX_DATA_LEN);
        prev = tmp;
    }
    rr_table_init(&pkt_pool, pkt_pool_space);
}

static struct peer_rx_ok_return
peer_rx_ok()
{
    struct peer_rx_ok_return result = {.new_gack = false, .new_data = false, .sleep_rcvd = false, .buf_emptied = false, .gacked_data = false};

    result.sleep_rcvd = IS_SLEEP_BITMAP(rcvd.sink_acked);
    if (result.sleep_rcvd) {
        return result;
    }

    // Only the reception of a new ACK bitmap is now considered new info

    // accepts pkts from nodes at same hop-distance
    if (rcvd.originator_id != SINK_ID_CONSTANT && rcvd.hop_counter >= node_dist) {
        if (!is_node_acked(node_acked, rcvd.originator_id)) {

          // if the pkt has been successfully added to the buffer, set new_data flag
          result.new_data |= rr_table_add(&pkt_pool, rcvd.originator_id, (uint8_t*) &rcvd, sizeof(info_t));

          // ... but update lhs only when hearing nodes closer to the sink
          // and the node's packet is in the buffer
          if (rcvd.hop_counter > node_dist && rr_table_contains(&pkt_pool, rcvd.originator_id)) {
              last_heard_originator_id = rcvd.originator_id;
          }
        }
        else {

          // the packet was already ACKed by the sink
          if (rcvd.hop_counter > node_dist) result.gacked_data = true;
        }
    }

    uint64_t nodes_to_remove = SET_DIFF(node_acked, rcvd.sink_acked);
    if (nodes_to_remove > 0) { // a new node has been acked
        node_acked |= rcvd.sink_acked;
        result.new_gack = true;

        // If the buffer was already empty avoid to set result.buf_emptied to true
        if (!pkt_pool_is_empty()){
            pkt_pool_remove_nodes(&nodes_to_remove);
            result.buf_emptied = pkt_pool_is_empty();
        }
    }

    rr_entry_t *sender_entry, *lhs_entry;
    sender_entry = rr_table_find(&pkt_pool, rcvd.originator_id);
    lhs_entry    = rr_table_find(&pkt_pool, rcvd.last_heard_originator_id);
    if (node_has_data &&
        rcvd.hop_counter < node_dist &&
        (sender_entry != NULL || lhs_entry != NULL)) {


        if ((lhs_entry != NULL &&
            lhs_entry->deadline == -1) || (lhs_entry != NULL &&
            lhs_entry->deadline == -1)) {

            uint16_t suppression_period = WEAVER_LOCAL_ACK_SUPPRESSION_INTERVAL(node_dist, global_ack_counter, GLOBAL_ACK_PERIOD);

#if WEAVER_WITH_FS
            uint16_t i;
            for (i=1; i<=suppression_period; ++i) {
                if (IS_FS(PA.logic_slot_idx + i)) {
                  suppression_period += FS_MACROSLOT;
                }
            }
#endif

            // directly perform the local ack to the entry
            if (sender_entry != NULL &&
                sender_entry->deadline == -1) {
                sender_entry->deadline = PA.logic_slot_idx + suppression_period;
            }
            if (lhs_entry != NULL &&
                lhs_entry->deadline == -1) {
                lhs_entry->deadline = PA.logic_slot_idx + suppression_period;
            }
        }
    }

    return result;
}

static inline void
pkt_pool_remove_nodes(const uint64_t* nodes_to_remove)
{
    size_t n_nodes_unmapped = unmap_nodes(nodes_to_remove,
            tmp_bitmap_unmapped, MAX_NODES_DEPLOYED);
    // remove nodes acked from pool
    size_t i = 0;
    for (; i < n_nodes_unmapped; i++) {
        rr_table_remove(&pkt_pool, tmp_bitmap_unmapped[i]);
    }
}

static inline void
pkt_pool_get_mypkt() {
    rr_entry_t *entry;
    entry = rr_table_find(&pkt_pool, node_id);
    node_has_data = false;
    if (entry == NULL) {
        return;
    }
    node_has_data = true;
    // check if the packet is the same (same originator_id)
    if (entry->originator_id == node_pkt.originator_id) {
        return; // avoid a wasteful copy. Tx the pkt the node is actually holding
    }

    // pkt is different, swap node_pkt
    if (sizeof(node_pkt) < entry->data_len) {
        ERR("Invalid pkt dimension.");
    }
    memcpy(&node_pkt, entry->data, entry->data_len);
}

static inline void
pkt_pool_get_next() {
    rr_entry_t *entry;
    entry = rr_table_get_next(&pkt_pool);
    node_has_data = false;
    if (entry == NULL) {
        return;
    }

    node_has_data = true;
    // check if the packet is the same (same originator_id)
    if (entry->originator_id == node_pkt.originator_id) {
        return; // avoid a wasteful copy. Tx the pkt the node is actually holding
    }

    // pkt is different, swap node_pkt
    if (sizeof(node_pkt) < entry->data_len) {
        ERR("Invalid pkt dimension.");
    }
    memcpy(&node_pkt, entry->data, entry->data_len);
}

static inline uint64_t
pkt_pool_get_sender_bitmap() {
    uint64_t snd_bitmap = 0x0;
    rr_entry_t *entry = pkt_pool.head_busy;
    for (; entry != NULL; entry = entry->next) {
        snd_bitmap = flag_node(snd_bitmap, entry->originator_id);
    }
    return snd_bitmap;
}

static inline bool
pkt_pool_is_empty() {
    return rr_table_is_empty(&pkt_pool);
}

static inline void
print_app_interactions() {
    size_t i = 0;
    for (; i < out_pnt; i++) {
        PRINTF("OUT %lu, O %d, S %d\n", logging_context, out_buf[i].originator_id, out_buf[i].seqno);
    }
    i = 0;
    for (; i < in_pnt; i++) {
        PRINTF("IN %lu, O %d, S %d, I %d\n", logging_context, in_buf[i].originator_id, in_buf[i].seqno, in_buf[i].slot_idx);
    }
}

static inline int16_t
infer_global_ack_counter(const uint32_t nslot, const uint8_t node_hop_dist)
{
    if (node_hop_dist > nslot) {
        WARN("node_hop_dist > nslot");
        return 0;
    }

#if WEAVER_WITH_FS
    int32_t ris = nslot - node_hop_dist - 3 * (WEAVER_BOOT_REDUNDANCY - 1);

    WARNIF(ris < -3*GLOBAL_ACK_PERIOD);

    if (ris < 0)
      return 3*GLOBAL_ACK_PERIOD + (ris % (3*GLOBAL_ACK_PERIOD)) ;

    return ris % (3 * GLOBAL_ACK_PERIOD);
#else
    return (nslot - node_hop_dist) % (3 * GLOBAL_ACK_PERIOD);
#endif
}

static inline void
update_termination(uint8_t node_hop_correction, const int16_t global_ack_counter, const int boot_redundancy_counter, const struct peer_rx_ok_return rx_updates)
{
  if (rx_updates.new_data || (!pkt_pool_is_empty() && rx_updates.new_gack) || rx_updates.buf_emptied) {
    termination_counter = 0;
    if (!ENABLE_EARLY_PEER_TERMINATION) node_hop_correction = 0;
    if (!pkt_pool_is_empty()) {
      termination_cap = 2 * 3 * GLOBAL_ACK_PERIOD - global_ack_counter + 3 * (SINK_RADIUS - node_hop_correction);
    }
    else {
      termination_cap = 3 * GLOBAL_ACK_PERIOD - global_ack_counter + 3 * (SINK_RADIUS - node_hop_correction);
    }
    termination_cap += 3 * boot_redundancy_counter;
    termination_cap += TERMINATION_WAIT_PEER;
  }
  else {
    termination_counter += 1;
  }
}
