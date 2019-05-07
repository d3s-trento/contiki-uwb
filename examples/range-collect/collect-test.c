/*
 * Copyright (c) 2017, University of Trento.
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

#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "dw1000.h"
#include "dw1000-ranging.h"
#include "core/net/linkaddr.h"
#include "net/rime/unicast.h"
/*---------------------------------------------------------------------------*/
PROCESS(range_process, "Test range process");
PROCESS(collect_process, "Test collect process");
PROCESS(unicast_process, "Test unicast process");
/*AUTOSTART_PROCESSES(&range_process); */
/*AUTOSTART_PROCESSES(&unicast_process); */
/*AUTOSTART_PROCESSES(&collect_process); */
AUTOSTART_PROCESSES(&collect_process, &range_process);
/*AUTOSTART_PROCESSES(&collect_process, &range_process, &unicast_process); */
/*---------------------------------------------------------------------------*/
#define RANGING_PERIOD (10 * CLOCK_SECOND)
#define RANGING_TIMEOUT (0.5 * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
#define COLLECT_SENDER_IPI (5 * CLOCK_SECOND)
#define COLLECT_NUM_RTX 15
/*---------------------------------------------------------------------------*/
#define UNICAST_IPI (2 * CLOCK_SECOND)
#define UNICAST_CHANNEL 0xAA
#define COLLECT_CHANNEL 0xBB
/*---------------------------------------------------------------------------*/
linkaddr_t sink = {{0x19, 0x15}}; /* node 1 */
/*---------------------------------------------------------------------------*/
enum {
  WARMUP_MSG = 0,
  NORMAL_MSG,
};
/*---------------------------------------------------------------------------*/
typedef struct test_msg {
  uint16_t seqn;
  uint8_t type;
  uint8_t num_nbr;
  linkaddr_t parent;
  uint16_t parent_etx;
  uint16_t current_rt_metric;
  int16_t parent_distance;
} test_msg_t;
/*---------------------------------------------------------------------------*/
static struct collect_conn tc;
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
static test_msg_t msg;
static struct collect_neighbor *nbr = NULL;
int16_t parent_distance;
/*---------------------------------------------------------------------------*/
static int total_recv = 0;
/*---------------------------------------------------------------------------*/
static void
col_recv(const linkaddr_t *originator, uint8_t seqn, uint8_t hops)
{
  if(packetbuf_datalen() != sizeof(msg)) {
    printf("Col warmup\n");
    return;
  }
  /* Copy the message */
  memcpy(&msg, packetbuf_dataptr(), sizeof(test_msg_t));
  total_recv++;
  printf("Col recv %02x%02x %u %u %u %02x%02x %u %u %d %u\n",
         originator->u8[0], originator->u8[1],
         msg.seqn, hops, msg.num_nbr,
         msg.parent.u8[0], msg.parent.u8[1], msg.parent_etx, msg.current_rt_metric,
         msg.parent_distance, total_recv);
}
/*---------------------------------------------------------------------------*/
static void
uc_recv(struct unicast_conn *conn, const linkaddr_t *from)
{
  uint16_t seqn;
  /* Copy the message */
  memcpy(&seqn, packetbuf_dataptr(), sizeof(seqn));
  printf("Unicast recv from %X.%X %u\n", from->u8[0], from->u8[1], seqn);
}
/*---------------------------------------------------------------------------*/
static void
uc_sent(struct unicast_conn *conn, int status, int num_tx)
{
  printf("%d.%d: uc_sent status: %d num_tx: %d\n",
         linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], status, num_tx);
}
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks col_callbacks = { .recv = col_recv };
static const struct unicast_callbacks uc_callbacks = { .recv = uc_recv, .sent = uc_sent };
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(collect_process, ev, data)
{
  static struct etimer periodic;
  static struct etimer et;
  static uint16_t seqn = 0;
  static int is_sink = 0;

  PROCESS_BEGIN();

  /* Open Collect Channel */
  collect_open(&tc, COLLECT_CHANNEL, COLLECT_ROUTER, &col_callbacks);

  if(linkaddr_cmp(&sink, &linkaddr_node_addr)) {
    is_sink = 1;
    collect_set_sink(&tc, 1);
  }

  etimer_set(&periodic, 10 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
  printf("Col node %02x%02x %d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], is_sink);

  if(!is_sink) {
    /* Allow some time for the network to settle */
    etimer_set(&et, 2 * 60 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    /* send some short packets to improve the tree */
    etimer_set(&et, 10 * 60 * CLOCK_SECOND);
    while(!etimer_expired(&et)) {
      etimer_set(&periodic, 60 * CLOCK_SECOND + random_rand() % (30 * CLOCK_SECOND));
      PROCESS_WAIT_EVENT();
      if(etimer_expired(&periodic)) {
        uint8_t a = 0;
        packetbuf_clear();
        packetbuf_copyfrom(&a, 1);
        collect_send(&tc, COLLECT_NUM_RTX);
      }
    }

    etimer_set(&periodic, COLLECT_SENDER_IPI);
    while(1) {

      etimer_set(&et, 10 + random_rand() % (COLLECT_SENDER_IPI - 20));
      PROCESS_WAIT_UNTIL(etimer_expired(&et));
      /* Build the message */
      msg.seqn = seqn;
      msg.num_nbr = collect_neighbor_list_num(&tc.neighbor_list);
      msg.current_rt_metric = tc.rtmetric;
      msg.parent_distance = parent_distance;
      linkaddr_copy(&msg.parent, &tc.parent);

      nbr = collect_neighbor_list_find(&tc.neighbor_list, &tc.parent);
      if(nbr != NULL) {
        msg.parent_etx = collect_neighbor_link_estimate(nbr);
      } else {
        msg.parent_etx = 0;
      }

      packetbuf_copyfrom(&msg, sizeof(test_msg_t));
      collect_send(&tc, COLLECT_NUM_RTX);
      printf("Col send %02x%02x %u\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], seqn);

      seqn++;
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
      etimer_reset(&periodic);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  static struct etimer periodic;
  static struct etimer et;
  static uint16_t uc_seqn = 0;

  PROCESS_BEGIN();

  unicast_open(&uc, UNICAST_CHANNEL, &uc_callbacks);
  etimer_set(&periodic, 2 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&periodic));

  if(linkaddr_cmp(&sink, &linkaddr_node_addr)) {
    printf("Node: I am unicast sink\n");
    while(1) {
      PROCESS_WAIT_EVENT();
    }
  } else {

    etimer_set(&periodic, UNICAST_IPI);
    while(1) {

      etimer_set(&et, random_rand() % UNICAST_IPI);
      PROCESS_WAIT_UNTIL(etimer_expired(&et));

      packetbuf_clear();
      packetbuf_copyfrom(&uc_seqn, sizeof(uc_seqn));
      printf("Node %02x%02x App: sending unicast %u\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], uc_seqn);
      unicast_send(&uc, &sink);

      uc_seqn++;
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
      etimer_reset(&periodic);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(range_process, ev, data)
{
  static struct etimer et;
  static struct etimer periodic;
  static struct etimer timeout;
  static int status;

  PROCESS_BEGIN();

  /* Allow some time for the network to settle. */
  etimer_set(&et, 1 * 60 * CLOCK_SECOND + (random_rand() % RANGING_PERIOD));
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  if(!linkaddr_cmp(&sink, &linkaddr_node_addr)) {
    etimer_set(&periodic, RANGING_PERIOD);
    while(1) {
      etimer_set(&et, random_rand() % RANGING_PERIOD);
      PROCESS_WAIT_UNTIL(etimer_expired(&et));
      if(!linkaddr_cmp(&tc.parent, &linkaddr_null)) {
        printf("RR 0x%02x%02x\n", tc.parent.u8[0], tc.parent.u8[1]);
        status = range_with(&tc.parent, DW1000_RNG_DS);
        if(!status) {
          printf("R req failed\n");
        } else {
          etimer_set(&timeout, RANGING_TIMEOUT);
          PROCESS_YIELD_UNTIL((ev == ranging_event || etimer_expired(&timeout)));
          if(etimer_expired(&timeout)) {
            printf("R TIMEOUT\n");
          } else if(((ranging_data_t *)data)->status) {
            printf("R success %f\n", ((ranging_data_t *)data)->distance);
            parent_distance = (int16_t)(((ranging_data_t *)data)->distance * 1000);
          } else {
            printf("R FAIL\n");
          }
        }
      }
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
      etimer_reset(&periodic);
    }
  } else {
    while(1) {
      PROCESS_WAIT_EVENT();
    }
  }
  PROCESS_END();
}
