/*
 * Copyright (c) 2019, University of Trento.
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

/**
 * \file
 *      Glossy test implementation
 *
 * \author
 *      Diego Lobba <diego.lobba@gmail.com>
 */

/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#include "sys/autostart.h"
#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "sys/node-id.h"
/*---------------------------------------------------------------------------*/
#include "glossy.h"
#include "dw1000-util.h"
#include "dw1000-arch.h"
#include "dw1000.h"
/*---------------------------------------------------------------------------*/
#include "deployment.h"
/*---------------------------------------------------------------------------*/
#ifndef INITIATOR_ID
#error No initiator id given. Define the INITIATOR_ID macro
#endif /* INITIATOR_ID */
/*---------------------------------------------------------------------------*/
//#define GLOSSY_PERIOD                   (RTIMER_SECOND * 10)        /* 10 seconds */
#define GLOSSY_PERIOD                   (RTIMER_SECOND / 5)      /* 200 milliseconds */
//#define GLOSSY_T_SLOT                   (RTIMER_SECOND / 33)        /* 30 ms*/
#define GLOSSY_T_SLOT                   (RTIMER_SECOND / 50)       /* 20 ms*/
#define GLOSSY_T_GUARD                  (RTIMER_SECOND / 1000)     /* 1ms */
//#define GLOSSY_N_TX                     2
/*---------------------------------------------------------------------------*/
#ifdef GLOSSY_TEST_CONF_PAYLOAD_DATA_LEN
#define PAYLOAD_DATA_LEN                GLOSSY_TEST_CONF_PAYLOAD_DATA_LEN
#else
#define PAYLOAD_DATA_LEN                115
#endif


#ifndef START_DELAY_INITIATOR
#define START_DELAY_INITIATOR 0
#endif

#ifndef START_DELAY_RECEIVER
#define START_DELAY_RECEIVER 0
#endif


#ifdef APP_CONF_FREQ_ADJ
#define APP_FREQ_ADJ APP_CONF_FREQ_ADJ
#else
#define APP_FREQ_ADJ 1
#endif


#define PREPARATION_DELAY_DTU (GLOSSY_START_DELAY_4NS + 100*1000/4) // add 100 us
#define PREPARATION_DELAY_RT  20 // ~ 600 us (TODO: convert the above value instead)


/*---------------------------------------------------------------------------*/
/*                          PRINT MACRO DEFINITIONS                          */
/*---------------------------------------------------------------------------*/
#if UWB_CONTIKI_PRINT_DEF
#include "print-def.h"
#pragma message STRDEF(INITIATOR_ID)
#pragma message STRDEF(PAYLOAD_DATA_LEN)
#pragma message STRDEF(GLOSSY_N_TX)
#pragma message STRDEF(GLOSSY_PERIOD)
#pragma message STRDEF(GLOSSY_T_SLOT)
#pragma message STRDEF(GLOSSY_T_GUARD)
#pragma message STRDEF(START_DELAY_INITIATOR)
#pragma message STRDEF(START_DELAY_RECEIVER)
#pragma message STRDEF(APP_FREQ_ADJ)
#endif
/*---------------------------------------------------------------------------*/
#define WAIT_UNTIL(time) \
{\
  rtimer_set(&g_timer, (time), 0, (rtimer_callback_t)glossy_thread, rt);\
  PT_YIELD(&glossy_pt);\
}
/*---------------------------------------------------------------------------*/
typedef struct {
  uint32_t seq_no;
  uint8_t  data[PAYLOAD_DATA_LEN];
}
__attribute__((packed))
glossy_data_t;
/*---------------------------------------------------------------------------*/
static struct pt      glossy_pt;
static struct rtimer  g_timer;
static glossy_data_t  glossy_payload;
static glossy_data_t  previous_payload;
static uint8_t        bootstrapped = 0;
static uint16_t       bootstrap_cnt = 0;
static uint16_t       pkt_cnt = 0;
static uint16_t       miss_cnt = 0;
static rtimer_clock_t previous_t_ref;
static uint32_t       previous_t_ref_dtu;
static uint32_t       glossy_period_dtu;
static uint32_t       start_time_dtu;
static rtimer_clock_t t_ref;

static float ppm_offset_filtered;
static bool  ppm_filter_bootstrapped;
/*---------------------------------------------------------------------------*/
// Contiki Process definition
PROCESS(glossy_test, "Glossy test");
AUTOSTART_PROCESSES(&glossy_test);
/*---------------------------------------------------------------------------*/
static unsigned short int initiator_id = INITIATOR_ID;
static uint8_t password[]   = {0x0, 0x0, 0x4, 0x2};
static size_t  password_len = 0;
static bool    password_set = false;
/*---------------------------------------------------------------------------*/
static bool password_check(
        const uint8_t *payload_data, const size_t payload_len,
        const uint8_t *password, const size_t password_len);
/*---------------------------------------------------------------------------*/



PT_THREAD(glossy_thread(struct rtimer *rt))
{

    PT_BEGIN(&glossy_pt);

    printf("Starting Glossy. Node ID %hu\n", node_id);

    previous_t_ref     = 0;
    previous_t_ref_dtu = 0;
    glossy_period_dtu = ((float)GLOSSY_PERIOD / RTIMER_SECOND) * 1000000000 / 4.00641;

    printf("Period DTU %lu\n", glossy_period_dtu);

    start_time_dtu = dwt_readsystimestamphi32() + PREPARATION_DELAY_DTU;

    while (1) {

        if(node_id == initiator_id) { /* ----------------------- initiator -- */

            //printf("RT %lu, DTU %lu\n", rt->time, dwt_readsystimestamphi32());
            glossy_start(node_id,
                    (uint8_t*)&glossy_payload,
                    sizeof(glossy_data_t),
                    GLOSSY_N_TX,
                    GLOSSY_WITH_SYNC,
                    true, start_time_dtu);

            WAIT_UNTIL(rt->time + GLOSSY_T_SLOT);
            glossy_stop();

            printf("[GLOSSY_BROADCAST]sent_seq %"PRIu32", payload_len %u\n",
                    glossy_payload.seq_no,
                    sizeof(glossy_data_t));
            printf("[GLOSSY_PAYLOAD]rcvd_seq %"PRIu32"\n", glossy_payload.seq_no);
            printf("[APP_STATS]n_rx %"PRIu8", n_tx %"PRIu8", f_relay_cnt %"PRIu8", "
                    "rcvd %"PRIu16", missed %"PRIu16", bootpd %"PRIu16"\n",
                    glossy_get_n_rx(), glossy_get_n_tx(),
                    glossy_get_relay_cnt_first_rx(),
                    pkt_cnt,
                    miss_cnt,
                    bootstrap_cnt);

            // print info to gather stats
            glossy_debug_print();
            glossy_stats_print();

            // print difference between the reference time among two
            // consecutive packets, ignore the first packet
            if (previous_payload.seq_no > 0 &&
                    glossy_payload.seq_no == previous_payload.seq_no + 1) {

                printf("[APP_DEBUG]Epoch_diff rtimer %"PRIu32", "
                        "dtu %"PRIu32"\n",
                        glossy_get_t_ref() - previous_t_ref,
                        glossy_get_t_ref_dtu() - previous_t_ref_dtu);

            }

            printf("[PPMA]seqno %lu, fppm 0, tppm 0, tref %lu, code %u\n", 
                glossy_payload.seq_no,
                glossy_get_t_ref_dtu(),
                dwt_getxtaltrim());

            previous_t_ref     = glossy_get_t_ref();
            previous_t_ref_dtu = glossy_get_t_ref_dtu();

            previous_payload = glossy_payload;

            start_time_dtu += glossy_period_dtu;
            glossy_payload.seq_no++;
            WAIT_UNTIL(glossy_get_t_ref() + GLOSSY_PERIOD - PREPARATION_DELAY_RT);

        } else { /* --------------------------------------------- receiver -- */

            if(!bootstrapped) {
                printf("BOOTSTRAP\r\n");
                bootstrap_cnt++;
                do {
                    glossy_start(GLOSSY_UNKNOWN_INITIATOR, (uint8_t*)&glossy_payload,
                            GLOSSY_UNKNOWN_PAYLOAD_LEN,
                            GLOSSY_N_TX, GLOSSY_WITH_SYNC,
                            false, 0);
                    WAIT_UNTIL(rt->time + GLOSSY_T_SLOT);
                    glossy_stop();
                } while(!glossy_is_t_ref_updated());
                /* synchronized! */
                bootstrapped = 1;
            } else {
                /* already synchronized, receive a packet */
                glossy_start(GLOSSY_UNKNOWN_INITIATOR, (uint8_t*)&glossy_payload,
                        GLOSSY_UNKNOWN_PAYLOAD_LEN,
                        GLOSSY_N_TX, GLOSSY_WITH_SYNC,
                        false, 0);
                WAIT_UNTIL(rt->time + GLOSSY_T_SLOT + GLOSSY_T_GUARD);
                glossy_stop();
            }

            /* has the reference time been updated? */
            if(glossy_is_t_ref_updated()) {
                /* sync received */
                //MINE
                printf("[APP_DEBUG]Synced\n");
                t_ref = glossy_get_t_ref() + GLOSSY_PERIOD;
            } else {
                /* sync missed */
                // MINE
                printf("[APP_DEBUG]Not Synced\n");
                t_ref += GLOSSY_PERIOD;
            }

            /* at least one packet received? */
            if(glossy_get_n_rx()) {

                pkt_cnt++;

                /*---------------------------------------------------------------*/
                // Check packet integrity
                /*---------------------------------------------------------------*/
                if (password_set && !password_check(glossy_payload.data, PAYLOAD_DATA_LEN,
                            password, password_len)) {

                    printf("[APP_DEBUG]Received a corrupted packet.\n");

                } else {

                    printf("[GLOSSY_PAYLOAD]rcvd_seq %"PRIu32"\n", glossy_payload.seq_no);
                    printf("[APP_STATS]n_rx %"PRIu8", n_tx %"PRIu8", f_relay_cnt %"PRIu8", "
                            "rcvd %"PRIu16", missed %"PRIu16", bootpd %"PRIu16"\n",
                            glossy_get_n_rx(), glossy_get_n_tx(),
                            glossy_get_relay_cnt_first_rx(),
                            pkt_cnt,
                            miss_cnt,
                            bootstrap_cnt);

                    // print info to compute stats
                    glossy_debug_print();
                    glossy_stats_print();

                    // print difference between the reference time among two
                    // consecutive packets, ignore the first packet
                    if (previous_payload.seq_no > 0 &&
                        glossy_payload.seq_no == previous_payload.seq_no + 1) {

                        printf("[APP_DEBUG]Epoch_diff rtimer %"PRIu32", "
                                "dtu %"PRIu32"\n",
                                glossy_get_t_ref() - previous_t_ref,
                                glossy_get_t_ref_dtu() - previous_t_ref_dtu);

                    }

                    if (previous_payload.seq_no > 0) {
                        uint64_t estimated_delta = (glossy_payload.seq_no-previous_payload.seq_no)*glossy_period_dtu;
                        uint32_t estimated_t_ref_dtu = previous_t_ref_dtu + estimated_delta;
                        float tppm = -(float)((int32_t)(glossy_get_t_ref_dtu()-estimated_t_ref_dtu))/estimated_delta*1000000;
                        uint8_t current_trim_code = dwt_getxtaltrim();

                        printf("[PPMA]seqno %lu, fppm %f, tppm %f, tref %lu, code %u\n", 
                            glossy_payload.seq_no,
                            glossy_get_ppm_offset(), 
                            tppm,
                            glossy_get_t_ref_dtu(),
                            current_trim_code);

#if APP_FREQ_ADJ
                        
                        if (ppm_filter_bootstrapped) {
                            ppm_offset_filtered = 
                            ppm_offset_filtered + 0.2*(tppm - ppm_offset_filtered);
                        }
                        else {
                            ppm_offset_filtered = tppm;
                            ppm_filter_bootstrapped = true;
                        }
                        uint8_t new_trim_code = dw1000_get_best_trim_code(ppm_offset_filtered,
                                                                  current_trim_code);
                        
                        if (new_trim_code != current_trim_code) {
                            dw1000_spi_set_slow_rate();
                            dwt_setxtaltrim(new_trim_code);
                            dw1000_spi_set_fast_rate();
                        }
#endif
                    }

                    previous_t_ref     = glossy_get_t_ref();
                    previous_t_ref_dtu = glossy_get_t_ref_dtu();

                    previous_payload = glossy_payload;

                }
            }
            else { /* no packet received */
                miss_cnt++;
            }

            /*---------------------------------------------------------------*/

            WAIT_UNTIL(t_ref - GLOSSY_T_GUARD);

        }
    }
    PT_END(&glossy_pt);
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(glossy_test, ev, data)
{
    static struct etimer et;
    PROCESS_BEGIN();

    etimer_set(&et, CLOCK_SECOND);
    PROCESS_YIELD_UNTIL(etimer_expired(&et));

#ifdef OVERRIDE_NODE_ID
    #if (OVERRIDE_NODE_ID == 0)
      #error node ID cannot be zero
    #endif
    node_id = OVERRIDE_NODE_ID;
#else
    deployment_set_node_id_ieee_addr();
#endif

    if (node_id == 0) {
        printf("Node ID may not be 0\n");
        PROCESS_EXIT();
    }
    if (glossy_init() == GLOSSY_STATUS_FAIL) {
        printf("Glossy init failed\n");
        PROCESS_EXIT();
    }

    printf("Glossy successfully initialised\n");

    if (node_id == INITIATOR_ID)
        etimer_set(&et, START_DELAY_INITIATOR*CLOCK_SECOND);
    else
        etimer_set(&et, START_DELAY_RECEIVER*CLOCK_SECOND);

    PROCESS_YIELD_UNTIL(etimer_expired(&et));

    glossy_version_print();
    /*-----------------------------------------------------------------------*/
    /*                       GLOSSY TEST                                     */
    /*-----------------------------------------------------------------------*/
    printf("Payload used: %hu\n", PAYLOAD_DATA_LEN);

    // Add a password to the data payload to check for packet integrity
    glossy_payload.seq_no  = 0;
    password_len = sizeof(password) / sizeof(password_len);
    if (password_len > PAYLOAD_DATA_LEN) {
        printf("Password too large to be embedded within the app payload!\n");
        printf("Password not set!\n");
        password_set = false;
    } else {
        memcpy(glossy_payload.data, password, password_len * sizeof(uint8_t));
        password_set = true;
    }
    previous_payload = glossy_payload;

    /*-----------------------------------------------------------------------*/
    rtimer_set(&g_timer, RTIMER_NOW() + RTIMER_SECOND/2 , 0,
            (rtimer_callback_t)glossy_thread, NULL);
    /*-----------------------------------------------------------------------*/
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static bool
password_check(const uint8_t *payload_data, const size_t payload_len,
        const uint8_t *password, const size_t password_len)
{
    size_t i = 0;
    if (payload_len < password_len) {
        return false;
    }
    for (; i < password_len; i++) {
        if (payload_data[i] != password[i]) {
            return false;
        }
    }
    return true;
}
