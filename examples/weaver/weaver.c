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


#define WEAVER_LOG_VERBOSE 0   // disable logs
#if WEAVER_LOG_VERBOSE
#define WEAVER_LOG_PRINT() weaver_log_print()
#else
#define WEAVER_LOG_PRINT() do {} while(0);
#endif // WEAVER_LOG_VERBOSE

#undef TREX_STATS_PRINT
#define TREX_STATS_PRINT()\
    do {trexd_stats_get(&trex_stats);\
        PRINTF("E %u, TX %d, RX %d, TO %d, ER %d\n", logging_context,\
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
#define PERIOD_SINK ( 500000 * UUS_TO_DWT_TIME_32)                   // ~ 500ms
#endif // WEAVER_LOG_VERBOSE

//#define PERIOD_PEER (PERIOD_SINK - (1000 * UUS_TO_DWT_TIME_32))     // ~ 1ms less than sink period
#define PERIOD_PEER (PERIOD_SINK)

// These are now computed at runtime
#define SLOT_DURATION (3000*UUS_TO_DWT_TIME_32)                     // ~ 3 ms
#define TIMEOUT (SLOT_DURATION - 1000*UUS_TO_DWT_TIME_32)           // slot timeout

#define JITTERINO_STEP (0x2) // DWT32 LSB is 4ns, 8ns is therefore 0x2, or 1 << 1
#define MAX_JITTERINO_MULT  (125) // 1us/8ns

// #define WEAVER_MAX_JITTERONE (20 * UUS_TO_DWT_TIME_32)
#ifndef WEAVER_MAX_JITTERONE
#define WEAVER_MAX_JITTERONE -1 // don't use fixed delay to transmit non-data packets, use jitterino
#endif

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

/* TBC: maybe (2 * (H - 2) - 1 + H + 3 * Y -\
 * (C + H + 2 * (H - 2)) % (3 * Y)) might be more accurate.
 * We need to test it.*/
#define WEAVER_LOCAL_ACK_SUPPRESSION_INTERVAL(H, C, Y) \
    (2 * (H - 2) + H + 3 * Y -\
     (C + H + 2 * (H - 2)) % (3 * Y))

#ifndef TERMINATION_WAIT_PEER
#define TERMINATION_WAIT_PEER                   (3) // + 3 * GLOBAL_ACK_PERIOD)
#endif // TERMINATION_WAIT_PEER

#ifndef TERMINATION_WAIT_SINK
#define TERMINATION_WAIT_SINK                   (3) // + 3 * GLOBAL_ACK_PERIOD)
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
#define WEAVER_PEER_TERMINATION_COUNT      (3 * SINK_RADIUS + 3 * GLOBAL_ACK_PERIOD + 3* WEAVER_BOOT_REDUNDANCY + TERMINATION_WAIT_PEER)
#endif  // WEAVER_PEER_TERMINATION_COUNT

#ifndef WEAVER_SINK_TERMINATION_COUNT
#define WEAVER_SINK_TERMINATION_COUNT      (3 * SINK_RADIUS + 3* WEAVER_BOOT_REDUNDANCY)
#endif  // WEAVER_SINK_TERMINATION_COUNT

#ifndef EXTRA_PAYLOAD_LEN
#define EXTRA_PAYLOAD_LEN                       0
#endif  // EXTRA_PAYLOAD_LEN

#define PKT_POOL_LEN                            35

#pragma message STRDEF(WEAVER_MAX_JITTERONE)
#pragma message STRDEF(WEAVER_N_ORIGINATORS)
#pragma message STRDEF(WEAVER_EPOCHS_PER_CYCLE)
#pragma message STRDEF(WEAVER_APP_START_EPOCH)
#pragma message STRDEF(SINK_ID)
#pragma message STRDEF(SINK_RADIUS)
#pragma message STRDEF(WEAVER_BOOT_REDUNDANCY)
#pragma message STRDEF(PKT_POOL_LEN)
#pragma message STRDEF(EXTRA_PAYLOAD_LEN)

// keep information about packet delivered to the app
// and packet required to be spread by weaver.
// App <-> weaver
typedef struct data_log_t {
    uint16_t originator_id;
    uint16_t seqno;
    uint16_t slot_idx;  // slot of reception, only meaningful for receptions
} data_log_t;

static data_log_t in_buf[50];
static size_t in_pnt = 0;
static data_log_t out_buf[50];
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
static inline uint8_t infer_global_ack_counter(const uint8_t slot_idx, const uint8_t node_hop_dist);
static inline void update_termination(
  uint8_t node_hop_correction,
  const uint8_t global_ack_counter,
  const int boot_redundancy_counter,
  const struct peer_rx_ok_return rx_updates);
static void print_table();

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

static uint16_t epoch_max_slot;
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

/*---------------------------------------------------------------------------*/
// moved here for visibility
static uint8_t global_ack_counter;          // [DN10C] keep track of the ACK period
/*---------------------------------------------------------------------------*/
static char sink_thread() {
    static uint16_t epoch;
    static uint16_t ntx_slot;
    static uint16_t nrx_slot;
    static int  boot_redundancy_counter;

    PT_BEGIN(&pt);

    node_dist = 0;
    epoch = 0;
    while (1) {
        out_pnt = 0; in_pnt = 0;
        memset(in_buf, 0, sizeof(in_buf));
        memset(out_buf, 0, sizeof(out_buf));
        STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
        weaver_log_init();
        epoch ++;                       // start from epoch 1
        node_acked = 0x0;               // forget previous ack
        global_ack_counter = 0;
        termination_counter = 0;
        termination_cap = WEAVER_SINK_TERMINATION_COUNT;
        boot_redundancy_counter = WEAVER_BOOT_REDUNDANCY;
        do {
            ntx_slot = 0;
            nrx_slot = 0;
            node_pkt.epoch         = epoch;
            node_pkt.originator_id = SINK_ID_CONSTANT;
            node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
            node_pkt.hop_counter   = node_dist;
            node_pkt.sink_acked    = node_acked;

            memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
            do {
                TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                global_ack_counter = (global_ack_counter + 1) % (3 * GLOBAL_ACK_PERIOD);
                termination_counter += 1;

                log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                    .node_dist = node_dist,
                    .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                    .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                weaver_log_append(&log);

                ntx_slot ++;
            } while (ntx_slot < WEAVER_NTX);

            do {
                TSM_RX_SLOT(&pt, buffer);
                nrx_slot++;

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
                            .seqno = rcvd.seqno, .slot_idx = PA.slot_idx};
                        in_pnt += in_pnt < 50 ? 1 : 0;
                    }
                    else {
                        termination_counter += 1;
                    }

                    log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
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

                global_ack_counter = (global_ack_counter + 1) % (3 * GLOBAL_ACK_PERIOD);

            } while (nrx_slot < WEAVER_NRX);

            boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0 : boot_redundancy_counter - 1;

        } while (PA.slot_idx < epoch_max_slot && termination_counter < termination_cap);

        // TX sleep command in a Glossy-like manner if time allows.
        // Prepare a packet with all bitmap flagged, to signal the network
        // is going to sleep.
        node_pkt.epoch         = epoch;
        node_pkt.originator_id = SINK_ID_CONSTANT;
        node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
        node_pkt.hop_counter   = node_dist;
        node_pkt.sink_acked    = BITMAP_ALL_ONE(typeof(node_pkt.sink_acked));
        memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

        ntx_slot = 0;
        while (PA.slot_idx < epoch_max_slot && ntx_slot < WEAVER_SLEEP_NTX) {
            TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN);

            log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                .node_dist = node_dist,
                .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                .acked = node_pkt.sink_acked, .buffer = pkt_pool_get_sender_bitmap()};
            weaver_log_append(&log);

            ntx_slot ++;
        }

        logging_context = epoch;
        printf("E %u, NSLOTS %d\n", logging_context, PA.slot_idx < 0 ? 0 : PA.slot_idx + 1);
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
    static uint16_t nrx_slot;
    static bool silent_tx;
    static bool is_originator;
    static bool is_sync = false;
    static int  n_missed_bootstrap = 0;
    static bool is_bootstrapped = false;
    static int  boot_redundancy_counter;        // be sure it's int...
    static struct peer_rx_ok_return rx_updates; // received something new in the very current slot
    static bool node_pkt_bootstrapped;          // set to true iff originators packet has been globally acked within the bootstrap phase
    static bool new_gack_last_tx;         // [DN10C] flag for global ACKs reception
    static bool i_tx;                           // [DN10C] force TX in next iteration
    static uint16_t seqno;
    static bool leak;                           // set to true when receiving a packet from a previous epoch in the scanning phase
    static bool must_sleep;                     // set to true when the node receives a sleep command from the sink
    static bool resend_gack;                    // heard a packet that was already ACKed by sink; trigger transmission

    PT_BEGIN(&pt);

    is_originator = false;
    seqno = 0;
    leak = false;
    while (1) {
        out_pnt = 0; in_pnt = 0;
        memset(in_buf, 0, sizeof(in_buf));
        memset(out_buf, 0, sizeof(out_buf));
        STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start());
        weaver_log_init();
        pkt_pool_init();                     // clean and init pkt_pool
        node_acked = 0x0;
        node_dist = ((uint8_t) -1);
        node_has_data = false;
        last_heard_originator_id = SINK_ID_CONSTANT;
        boot_redundancy_counter = WEAVER_BOOT_REDUNDANCY;
        must_sleep = false;

        if (is_originator && WEAVER_BOOT_REDUNDANCY > 0) {
            boot_redundancy_counter += 1;
            node_pkt_bootstrapped = false;
        }

        if (is_originator) {
            seqno ++;
            node_pkt.seqno = seqno;             // unknown epoch atm
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
            out_pnt += out_pnt < 50 ? 1 : 0;
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
        while (1) {

            if (is_sync) {
                TSM_RX_SLOT(&pt, buffer);
                termination_counter += 1;
            }
            else {
                TSM_SCAN(&pt, buffer);
            }


            if (PA.status == TREX_RX_SUCCESS) {
                // request TSM to resynchronise using the received packet
                NA.accept_sync = 1;

                memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                if (rcvd.originator_id != SINK_ID_CONSTANT) {
                  memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
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

                log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                    .node_dist = node_dist,
                    .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                    .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                weaver_log_append(&log);

                is_bootstrapped = true;
                n_missed_bootstrap = 0;
                break;
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
            // If the slot has been shrinked it's not possible to keep these
            // guards bigger. Keep the default value set in TSM.
            //NA.rx_guard_time = (100 * UUS_TO_DWT_TIME_32); // keep a longer guard
        }
        // Used in next epoch
        is_sync = n_missed_bootstrap >= WEAVER_MISSED_BOOTSTRAP_BEFORE_SCAN ? false : true;

        /*--------------------------------------------------------------------*/
        termination_cap = WEAVER_PEER_TERMINATION_COUNT;
        termination_counter = 0;
        silent_tx = false;
        i_tx = false;

        global_ack_counter = infer_global_ack_counter(PA.slot_idx + 1, node_dist);

        while (is_bootstrapped && !must_sleep && PA.slot_idx < epoch_max_slot &&
            termination_counter <= termination_cap) {
            ntx_slot = 0;
            nrx_slot = 0;
            resend_gack = false;

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
                do {

                    // always use jitterino
                    NA.tx_delay = MAX_JITTERINO_MULT ? ((random_rand() % (MAX_JITTERINO_MULT+1))*JITTERINO_STEP) : 0;

                    if (!node_has_data && WEAVER_MAX_JITTERONE > 0) {
                        // peers transmit non-data pkts (i.e. global-acks) with a fixed jitterone
                        // in addition to jitterino
                        NA.tx_delay += WEAVER_MAX_JITTERONE;
                    }

                    if (node_has_data) { TSM_TX_SLOT(&pt, buffer, sizeof(info_t)); }
                    else { TSM_TX_SLOT(&pt, buffer, sizeof(info_t) - EXTRA_PAYLOAD_LEN); }

                    termination_counter += 1;
                    boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0 : boot_redundancy_counter - 1; // just to avoid underflow --- TBC
                    new_gack_last_tx = false;

                    // prepare log, keeping the originator_id that was set when preparing the packet
                    log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = log.originator_id, // keep the same originator_id, since it was set just before TX
                        .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};

                    weaver_log_append(&log);

                    ntx_slot ++;
                } while (ntx_slot < WEAVER_NTX);
                last_heard_originator_id = SINK_ID_CONSTANT; // TBC --- Is the best choice to reset lhs after tx the previous one?
            }
            else {
                silent_tx = false;
                last_heard_originator_id = SINK_ID_CONSTANT; // TBC ---  Is the best choice to reset lhs before starting rx?

                TSM_RX_SLOT(&pt, buffer);

                if (PA.status == TREX_RX_SUCCESS) {

                    memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                    if (rcvd.originator_id != SINK_ID_CONSTANT) {
                      memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
                    }

                    rx_updates = peer_rx_ok();
                    must_sleep |= rx_updates.sleep_rcvd;
                    new_gack_last_tx |= rx_updates.new_gack;
                    resend_gack |= rx_updates.gacked_data;

                    log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                    weaver_log_append(&log);

                    update_termination(node_dist, global_ack_counter, boot_redundancy_counter, rx_updates);
                }
                else if (PA.status == TREX_RX_ERROR) {  // NOTE: it's unclear when TREX_RX_MALFORMED occurs
                    termination_counter = 0;
                }
                else if (PA.status == TREX_RX_TIMEOUT) {
                    termination_counter += 1;
                }
            }

            global_ack_counter = (global_ack_counter + 1) % (3 * GLOBAL_ACK_PERIOD);

            // RX slots of the schedule;
            // now lhs is either SINK_ID_CONSTANT or some originator_id received in the overhearing slot
            do {
                silent_tx = false;
                TSM_RX_SLOT(&pt, buffer);

                if (PA.status == TREX_RX_SUCCESS) {

                    memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t) - EXTRA_PAYLOAD_LEN);
                    if (rcvd.originator_id != SINK_ID_CONSTANT) {
                      memcpy(&rcvd, buffer + TSM_HDR_LEN, sizeof(info_t));
                    }

                    rx_updates = peer_rx_ok();
                    must_sleep |= rx_updates.sleep_rcvd;
                    new_gack_last_tx |= rx_updates.new_gack;
                    resend_gack |= rx_updates.gacked_data;

                    log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                        .node_dist = node_dist,
                        .originator_id = rcvd.originator_id, .lhs = last_heard_originator_id,
                        .acked = node_acked, .buffer = pkt_pool_get_sender_bitmap()};
                    weaver_log_append(&log);

                    update_termination(node_dist, global_ack_counter, boot_redundancy_counter, rx_updates);

                    if (rcvd.hop_counter + 1 < node_dist) {
                        node_dist = rcvd.hop_counter + 1;
                        // recompute the local global ack counter based
                        // on the new node distance from the sink
                        global_ack_counter = infer_global_ack_counter(PA.slot_idx + 1, node_dist);
                        break;
                    }
                }
                else if (PA.status == TREX_RX_ERROR) {
                    termination_counter = 0;   // we could think of a simple -1 rather than a complete reset
                }
                else if (PA.status == TREX_RX_TIMEOUT) {
                    termination_counter += 1;
                }

                global_ack_counter = (global_ack_counter + 1) % (3 * GLOBAL_ACK_PERIOD);

                nrx_slot++;
            } while (nrx_slot < WEAVER_NRX);

            // TBC ---
            if (is_originator &&
                (!node_pkt_bootstrapped) &&
                is_node_acked(node_acked, node_id)) {

                boot_redundancy_counter = boot_redundancy_counter <= 0 ? 0 : boot_redundancy_counter - 1; // just to avoid underflow
                node_pkt_bootstrapped = true;
            }

            if (is_originator && WEAVER_BOOT_REDUNDANCY > 0 && boot_redundancy_counter == 1) {
                // trasmit node's pkt in the B+1 TR slot during the bootstrapping phase
                pkt_pool_get_mypkt();
            }
            else {
                // update pkt_pool counters. If current slot > expected slot then
                // their counter is set to -1 and will be transmitted again in the future
                rr_table_update_deadlines(&pkt_pool, PA.slot_idx);
                pkt_pool_get_next();
            }

            // [DN10C] Boolean to force tranmission of ACK-only messages
            i_tx = (global_ack_counter == 0) && new_gack_last_tx;

            // Decide if the next TR slot should be silent (account for i_tx)
            if (!resend_gack && !i_tx && !node_has_data && boot_redundancy_counter <= 0) silent_tx = true;
        }

        // TX sleep command in a Glossy-like manner if time allows.
        // Prepare a packet with all bitmap flagged, to signal the network
        // is going to sleep.
        node_pkt.epoch         = epoch;
        node_pkt.originator_id = SINK_ID_CONSTANT;
        node_pkt.last_heard_originator_id = SINK_ID_CONSTANT;
        node_pkt.hop_counter   = node_dist;
        node_pkt.sink_acked    = BITMAP_ALL_ONE(typeof(node_pkt.sink_acked));
        memcpy(buffer+TSM_HDR_LEN, &node_pkt, sizeof(info_t));

        ntx_slot = 0;
        while (must_sleep && PA.slot_idx < epoch_max_slot && ntx_slot < WEAVER_SLEEP_NTX) {
            TSM_TX_SLOT(&pt, buffer, sizeof(info_t));

            log = (weaver_log_t) {.idx = PA.slot_idx, .slot_status = PA.status,
                .node_dist = node_dist,
                .originator_id = node_pkt.originator_id, .lhs = node_pkt.last_heard_originator_id,
                .acked = node_pkt.sink_acked, .buffer = pkt_pool_get_sender_bitmap()};
            weaver_log_append(&log);

            ntx_slot ++;
        }

        logging_context = epoch;
        if (is_originator) {
            printf("E %u, IS_ORIG\n", logging_context);
        }
        printf("E %u, NSLOTS %d\n", logging_context, PA.slot_idx < 0 ? 0 : PA.slot_idx + 1);
        PRINTF("BOOT %u, B %d, N %d, L %d\n", logging_context, is_bootstrapped, n_missed_bootstrap, leak);
        leak = false;
        print_acked(node_acked);
        static char label[] = "BUF";
        print_bitmap(label, 4, pkt_pool_get_sender_bitmap());
        print_app_interactions();
        TREX_STATS_PRINT();
        STATETIME_MONITOR(printf("STATETIME "); dw1000_statetime_print());
        WEAVER_LOG_PRINT();

        // APP-code:
        // Check if the node is an originator in the next epoch[thanks to crystal-test for the code]
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
    static uint8_t  framelength;
    static uint32_t frame_time_4ns;
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

    // use do while to define prefix array more than once
    do {
        char prefix[3] = "BO";
        print_bitmap(prefix, 3, tmp_bitmap); // Bitmap Order: list node ids accordingly to their position in the bitmap
    } while (0);
    do {
        char prefix[3] = "ME";
        tmp_bitmap = 0x0;
        tmp_bitmap = ack_node(tmp_bitmap, node_id);
        print_bitmap(prefix, 3, tmp_bitmap); // ERROR CHECK: print my node id using the bitmap
    } while (0);

    max_jitter_4ns = 0;
#if MAX_JITTERINO_MULT > 0
    max_jitter_4ns = JITTERINO_STEP * MAX_JITTERINO_MULT;
#endif
#if WEAVER_MAX_JITTERONE > 0
    max_jitter_4ns += WEAVER_MAX_JITTERONE;
#endif

    framelength = (sizeof(info_t) + sizeof(struct tsm_header) + 2); // 2 = TREXD_BYTE_OVERHEAD
    frame_time_4ns =
        (dw1000_estimate_tx_time(dw1000_get_current_cfg(), framelength, 0) + 20) / DWT_TICK_TO_NS_32;
    rx_timeout = frame_time_4ns + (10*UUS_TO_DWT_TIME_32) + max_jitter_4ns + (5*UUS_TO_DWT_TIME_32);
    slot_duration = ((dw1000_estimate_tx_time(dw1000_get_current_cfg(), framelength, 0) +
                2400 * framelength + 250000) / DWT_TICK_TO_NS_32) +
                max_jitter_4ns + ((15 + 5)*UUS_TO_DWT_TIME_32);
    epoch_max_slot = (uint16_t) ((double) PERIOD_SINK / slot_duration) - 10;

    tsm_set_default_preambleto(tsm_get_default_preambleto() +
          max_jitter_4ns + (5*UUS_TO_DWT_TIME_32)); // guards


    printf("RXTO %"PRIu32"\n", rx_timeout);
    printf("SLOT_DURATION %"PRIu32"\n", slot_duration);
    printf("MAX_SLOT_IDX %"PRIu16"\n", epoch_max_slot);

    if (node_id == SINK_ID) {
        PRINTF("IS_SINK\n");
        etimer_set(&et, CLOCK_SECOND * 10);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));
        tsm_start(slot_duration, rx_timeout, (tsm_slot_cb)sink_thread);
    }
    else {
        etimer_set(&et, CLOCK_SECOND * 2);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));
        tsm_start(slot_duration, rx_timeout, (tsm_slot_cb)peer_thread);
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

    // [DN10] Only the reception of a new ACK bitmap is now considered new info

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

        // directly perform the local ack to the entry
        /*
        TBC: If the packet has been already locally ACKed is it better
             to reset the counter to 0 (as a node closer to the sink is
             trying to transmit that packet) or not?
             Reset it to 0 will reduce contention, but also limit spatial diversity.
             For now we don't reset the counter. If we decide to reset the counter
             each time, probably it is worth to remove !node_has_data  from the previous
             if statement.
        */
        if (sender_entry != NULL &&
            sender_entry->deadline == -1) {
            // TODO: pick a fancier and possibly shorter name :(
            sender_entry->deadline = PA.slot_idx + WEAVER_LOCAL_ACK_SUPPRESSION_INTERVAL(node_dist, global_ack_counter, GLOBAL_ACK_PERIOD);
        }
        if (lhs_entry != NULL &&
            lhs_entry->deadline == -1) {
            lhs_entry->deadline = PA.slot_idx + WEAVER_LOCAL_ACK_SUPPRESSION_INTERVAL(node_dist, global_ack_counter, GLOBAL_ACK_PERIOD);
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
        PRINTF("Error: invalid pkt dimension\n"); // TODO: use new logging here
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
        PRINTF("Error: invalid pkt dimension\n"); // TODO: use new logging here
    }
    memcpy(&node_pkt, entry->data, entry->data_len);
}

static void print_table() {
    for (rr_entry_t *tmp=pkt_pool_space; tmp < pkt_pool_space + PKT_POOL_LEN; tmp++) {
        if (tmp == pkt_pool.cur_busy)
            printf("S %"PRIu16", DL %u, C %d, N %"PRIu16"  <--\n",
                tmp->originator_id, tmp->data_len, tmp->deadline, tmp->next == NULL ? 0 : tmp->next->originator_id);
        else
            printf("S %"PRIu16", DL %u, C %d, N %"PRIu16"\n",
                tmp->originator_id, tmp->data_len, tmp->deadline, tmp->next == NULL ? 0 : tmp->next->originator_id);
    }
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
        PRINTF("OUT %u, O %d, S %d\n", logging_context, out_buf[i].originator_id, out_buf[i].seqno);
    }
    i = 0;
    for (; i < in_pnt; i++) {
        PRINTF("IN %u, O %d, S %d, I %d\n", logging_context, in_buf[i].originator_id, in_buf[i].seqno, in_buf[i].slot_idx);
    }
}

static inline uint8_t
infer_global_ack_counter(const uint8_t nslot, const uint8_t node_hop_dist)
{
    if (node_hop_dist > nslot) {
        return 0;
    }
    return (nslot - node_hop_dist) % (3 * GLOBAL_ACK_PERIOD);
}

static inline void
update_termination(uint8_t node_hop_correction, const uint8_t global_ack_counter, const int boot_redundancy_counter, const struct peer_rx_ok_return rx_updates)
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
