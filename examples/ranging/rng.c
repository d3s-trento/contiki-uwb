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
/*---------------------------------------------------------------------------*/
PROCESS(range_process, "Test range process");
AUTOSTART_PROCESSES(&range_process);
/*---------------------------------------------------------------------------*/
#define RANGING_TIMEOUT (CLOCK_SECOND / 10)
#define APP_RADIO_CONF 1
/*---------------------------------------------------------------------------*/
dwt_config_t radio_config = {
#if APP_RADIO_CONF == 1
  .chan = 4,
  .prf = DWT_PRF_64M,
  .txPreambLength = DWT_PLEN_128,
  .dataRate = DWT_BR_6M8,
  .txCode = 17,
  .rxCode = 17,
  .rxPAC = DWT_PAC8,
  .nsSFD = 0 /* standard */,
  .phrMode = DWT_PHRMODE_STD,
  .sfdTO = (129 + 8 - 8),
#elif APP_RADIO_CONF == 2
  .chan = 2,
  .prf = DWT_PRF_64M,
  .txPreambLength = DWT_PLEN_1024,
  .dataRate = DWT_BR_110K,
  .txCode = 9,
  .rxCode = 9,
  .rxPAC = DWT_PAC32,
  .nsSFD = 1 /* non-standard */,
  .phrMode = DWT_PHRMODE_STD,
  .sfdTO = (1025 + 64 - 32),
#else
#error App: radio config is not set
#endif
};
/*---------------------------------------------------------------------------*/
linkaddr_t dst = {{0x18, 0x8c}};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(range_process, ev, data)
{
  static struct etimer et;
  static struct etimer timeout;
  static int status;

  PROCESS_BEGIN();

  dw1000_configure(&radio_config);
  printf("I am %02x%02x dst %02x%02x\n",
         linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1],
         dst.u8[0],
         dst.u8[1]);

  if(!linkaddr_cmp(&linkaddr_node_addr, &dst)) {

    etimer_set(&et, 5 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));
    printf("I am %02x%02x ranging with %02x%02x\n",
           linkaddr_node_addr.u8[0],
           linkaddr_node_addr.u8[1],
           dst.u8[0],
           dst.u8[1]);

    while(1) {
      printf("R req\n");
      status = range_with(&dst, DW1000_RNG_DS);
      if(!status) {
        printf("R req failed\n");
      } else {
        etimer_set(&timeout, RANGING_TIMEOUT);
        PROCESS_YIELD_UNTIL((ev == ranging_event || etimer_expired(&timeout)));
        if(etimer_expired(&timeout)) {
          printf("R TIMEOUT\n");
        } else if(((ranging_data_t *)data)->status) {
          ranging_data_t *d = data;
          printf("R success: %d bias %d\n", (int)(100*d->raw_distance), (int)(100*d->distance));
        } else {
          printf("R FAIL\n");
        }
      }
      etimer_set(&et, CLOCK_SECOND / 50);
      PROCESS_WAIT_UNTIL(etimer_expired(&et));
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
