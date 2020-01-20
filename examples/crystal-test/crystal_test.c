#include <stdio.h>

#include "crystal_test.h"
#include "crystal.h"
#include "node-id.h"
#include "etimer.h"

#include "sndtbl.c"

#include "contiki.h"
#include "deployment.h"

static app_t_payload t_payload;
static app_a_payload a_payload;

/* How many packets the "sensor" nodes will send per epoch */
#define PACKETS_PER_EPOCH 1

/* Enable/disable the application-level logging */
#define LOGGING 1

#define MS_TO_TICKS(v) ((uint32_t)RTIMER_SECOND*(v)/1000)

/* Crystal configuration structure */
static crystal_config_t conf;

PROCESS(crystal_test, "Crystal test");

static uint16_t app_have_packet;
static uint16_t app_seqn;
static uint16_t app_log_seqn;

static uint16_t n_pkt_sent;
static uint16_t n_pkt_recv;

static int is_sink; // whether this node is Crystal sink

static process_event_t EPOCH_END_EV;

#if LOGGING
struct out_pkt_record {
    uint16_t seqn;
    uint16_t acked;

} out_packets[PACKETS_PER_EPOCH];

#define RECV_PACKET_NUM 100
struct in_pkt_record {
    uint16_t src;
    uint16_t seqn;
} in_packets[RECV_PACKET_NUM];

#endif //LOGGING

/* Note that app_* Crystal callbacks are called in the interrupt context
 * between Crystal slots, so they should return ASAP.
 *
 * The callbacks are described in crystal.h.
 */

// Pre-S phase Crystal callback
uint8_t* app_pre_S() {
    app_have_packet = 0;
    //log_send_seqn = 0;
    n_pkt_sent = 0;
    n_pkt_recv = 0;
    return NULL;
}

static inline void app_new_packet() {
    app_seqn ++;
#if LOGGING
    out_packets[n_pkt_sent].seqn = app_seqn;
    out_packets[n_pkt_sent].acked = 0;
#endif //LOGGING
    n_pkt_sent ++;
}

static inline void app_mark_acked() {
#if LOGGING
    out_packets[n_pkt_sent-1].acked = 1;
#endif //LOGGING
}

// Post-S phase Crystal callback
void app_post_S(int received, uint8_t* payload) {
    if (is_sink)
        return;
// the sndtbl.c file is written by simgen_ta,
// it defines the list of sensor nodes allowed to
// generate new packets in the first T slot
#if CONCURRENT_TXS > 0
    int i;
    int cur_idx;
    if (crystal_info.epoch >= START_EPOCH) {
        cur_idx = ((crystal_info.epoch - START_EPOCH) % NUM_ACTIVE_EPOCHS) * CONCURRENT_TXS;
        for (i=0; i<CONCURRENT_TXS; i++) {
            if (node_id == sndtbl[cur_idx + i]) {
                app_have_packet = 1;
                app_new_packet();
                break;
            }
        }
    }
// if CONCURRENT_TXS is not defined, then let every receiver
// generate a new packet
#else
    app_new_packet();
    app_have_packet = 1;
#endif // CONCURRENT_TXS
}

// Pre-T phase Crystal callback
uint8_t* app_pre_T() {
    if (app_have_packet) {
        t_payload.seqn = app_seqn;
        t_payload.src  = node_id;
        crystal_app_log.send_seqn  = app_seqn;
        return (uint8_t*)&t_payload;
    }
    return NULL;
}

// Post-T, pre-A phase Crystal callback
uint8_t* app_between_TA(int received, uint8_t* payload) {
    if (received) {
        t_payload = *(app_t_payload*)payload;

        crystal_app_log.recv_src  = t_payload.src;
        crystal_app_log.recv_seqn = t_payload.seqn;
    }
    if (received && is_sink) {
        // fill in the ack payload
        a_payload.src  = t_payload.src;
        a_payload.seqn = t_payload.seqn;

#if LOGGING
        if (n_pkt_recv < RECV_PACKET_NUM) {
            in_packets[n_pkt_recv].src = t_payload.src;
            in_packets[n_pkt_recv].seqn = t_payload.seqn;
            n_pkt_recv ++;
        }
#endif
    }
    else {
        a_payload.seqn = NO_SEQN;
        a_payload.src  = NO_NODE;
    }
    return (uint8_t*)&a_payload;
}

// Post-A phase Crystal callback
void app_post_A(int received, uint8_t* payload) {
    // non-sink: if acked us, stop sending data
    crystal_app_log.acked = 0;
    if (app_have_packet && received) {
        a_payload = *(app_a_payload*)payload;

        if ((a_payload.src == node_id) && (a_payload.seqn == app_seqn)) {
            crystal_app_log.acked = 1;
            app_mark_acked();
            if (n_pkt_sent < PACKETS_PER_EPOCH) {
                app_new_packet();
            }
            else {
                app_have_packet = 0;
            }
        }
    }
}

// Called when Crystal goes to sleep (inactive portion of the epoch)
void app_epoch_end() {
    // "wake up" the main process
    process_post(&crystal_test, EPOCH_END_EV, NULL);
}


void app_crystal_start_done(bool success) {}

#ifndef START_DELAY_SINK
#define START_DELAY_SINK 0
#endif

#ifndef START_DELAY_NONSINK
#define START_DELAY_NONSINK 0
#endif


AUTOSTART_PROCESSES(&crystal_test);
PROCESS_THREAD(crystal_test, ev, data) {
    PROCESS_BEGIN();

    static struct etimer et;
    static bool ret;
    EPOCH_END_EV = process_alloc_event();

#ifdef NODE_ID
  node_id = NODE_ID;
#else
    // get node id from deployment file
    deployment_set_node_id_ieee_addr();
#endif

    is_sink = node_id == SINK_ID;

    crystal_init();

    if (is_sink)
        etimer_set(&et, START_DELAY_SINK*CLOCK_SECOND);
    else
        etimer_set(&et, START_DELAY_NONSINK*CLOCK_SECOND);

    PROCESS_YIELD_UNTIL(etimer_expired(&et));


    printf("I am alive! Node ID: %hu\n", node_id);
    conf = crystal_get_config();
    conf.plds_S  = sizeof(app_s_payload);
    conf.plds_T  = sizeof(app_t_payload);
    conf.plds_A  = sizeof(app_a_payload);
    conf.is_sink = is_sink;

    PRINT_CRYSTAL_CONFIG(conf);
    ret = crystal_start(&conf);
    if (!ret)
        printf("Crystal failed to start\n");

#if LOGGING
    while(1) {
        PROCESS_WAIT_EVENT();
        if (ev==EPOCH_END_EV) {
            int i;
            crystal_print_epoch_logs();
            if (is_sink) {
                for (i=0; i<n_pkt_recv; i++) {
                    printf("B %u:%u %u %u\n", crystal_info.epoch,
                            in_packets[i].src,
                            in_packets[i].seqn,
                            app_log_seqn);
                    app_log_seqn ++;
                }
            }
            else {
                for (i=0; i<n_pkt_sent; i++) {
                    printf("A %u:%u %u %u\n", crystal_info.epoch,
                            out_packets[i].seqn,
                            out_packets[i].acked,
                            app_log_seqn);
                    app_log_seqn ++;
                }
            }
        }
    }
#endif //LOGGING

    PROCESS_END();
}



void app_pre_epoch() {}
