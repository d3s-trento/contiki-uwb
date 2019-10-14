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

/*
 * \file
 *		EVB1000 Contiki Platform Main
 *
 * \author
 * 		Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "contiki.h"
#include "sys/clock.h"
#include "sys/rtimer.h"
#include "dev/watchdog.h"
#include "dev/radio.h"
#include "lib/random.h"
#include "net/netstack.h"
#include <stdio.h>
#include "serial-line.h"
/*---------------------------------------------------------------------------*/
/* For IPv6 Stack */
#include "net/queuebuf.h"
#include "net/ip/tcpip.h"
#include "net/ip/uip.h"
/*---------------------------------------------------------------------------*/
#include "board.h"
#include "leds.h"
#include "lcd.h"
#include "dw1000-arch.h"
#include "dw1000-config.h"
#include "deca_usb.h"
/*---------------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
/*---------------------------------------------------------------------------*/
/* DW1000 Radio Driver */
#include "deca_device_api.h"
/*---------------------------------------------------------------------------*/
static void
fade_leds(void)
{
  leds_off(LEDS_CONF_ALL);
  clock_wait(100);
  leds_on(LEDS_YELLOW);
  clock_wait(100);
  leds_on(LEDS_RED);
  clock_wait(100);
  leds_on(LEDS_GREEN);
  clock_wait(100);
  leds_on(LEDS_ORANGE);
  clock_wait(100);
  leds_on(LEDS_CONF_ALL);
  clock_wait(100);
  leds_off(LEDS_CONF_ALL);
  clock_wait(100);
}
/*---------------------------------------------------------------------------*/
/* This function must be called after initializing the DW1000 radio */
static void
configure_addresses(void)
{
  uint8_t ext_addr[8];
  uint32_t part_id, lot_id;

  /* Read from the DW1000 OTP memory the DW1000 PART and LOT IDs */
  part_id = dwt_getpartid();
  lot_id = dwt_getlotid();

  /* Compute the Link Layer addresses depending on the PART and LOT IDs */
  ext_addr[0] = (lot_id  & 0xFF000000) >> 24;
  ext_addr[1] = (lot_id  & 0x00FF0000) >> 16;
  ext_addr[2] = (lot_id  & 0x0000FF00) >> 8;
  ext_addr[3] = (lot_id  & 0x000000FF);
  ext_addr[4] = (part_id & 0xFF000000) >> 24;
  ext_addr[5] = (part_id & 0x00FF0000) >> 16;
  ext_addr[6] = (part_id & 0x0000FF00) >> 8;
  ext_addr[7] = (part_id & 0x000000FF);

  /* Populate linkaddr_node_addr (big-endian) */
  memcpy(&linkaddr_node_addr, &ext_addr[8 - LINKADDR_SIZE], LINKADDR_SIZE);

  NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, IEEE802154_PANID);
  NETSTACK_RADIO.set_object(RADIO_PARAM_64BIT_ADDR, ext_addr, 8);
  NETSTACK_RADIO.set_value(RADIO_PARAM_16BIT_ADDR, 
      (ext_addr[6]) << 8 | (ext_addr[7])); // converting from big-endian format
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Main function for EVB1000 platform
 */
int
main(void)
{
  rcc_init();
  gpio_init();
  nvic_init();
  rtc_init();

  /* Init Contiki Clock module */
  clock_init();

  /* Init EVB1000 LEDs */
  leds_init();
  leds_set(LEDS_CONF_ALL);

  /* Init USB as a VCP */
  usb_init();
  clock_wait(1000);
  leds_off(LEDS_CONF_ALL);

  /* Init rtimer based on RTC clock */
  rtimer_init();

  /* Init the independent watchdog */
  watchdog_init();

  /* Init EA DOGM 16x2 W-A LCD */
  lcd_init();

  //lpm_init();

  process_init();

  process_start(&etimer_process, NULL);
  
  ctimer_init();

  energest_init();
  ENERGEST_ON(ENERGEST_TYPE_CPU);

  /* Init network stack */
  netstack_init();

  /* Set the link layer addresses and the PAN ID */
  configure_addresses();

#if NETSTACK_CONF_WITH_IPV6
  memcpy(&uip_lladdr.addr, &linkaddr_node_addr, sizeof(uip_lladdr.addr));
  queuebuf_init();
  process_start(&tcpip_process, NULL);
#endif /* NETSTACK_CONF_WITH_IPV6 */

  /* Init pseudo-random generator with a chip-id based seed */ 
  random_init(0xFFFF & dwt_getpartid());

  //process_start(&sensors_process, NULL);

  serial_line_init();
  fade_leds();

  char str[20];
#if (LINKADDR_SIZE == 2)
  snprintf(str, 20, "%02x%02x", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
#else
  snprintf(str, 20, "%02x%02x%02x%02x%02x%02x%02x%02x", 
      linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
      linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[3],
      linkaddr_node_addr.u8[4], linkaddr_node_addr.u8[5],
      linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
#endif
  lcd_display_str(str);

  /* Start application processes */
  autostart_start(autostart_processes);

  /* Start Independent WDG */
  watchdog_start();

  while(1) {
    uint8_t r;
    do {
      r = process_run();
      watchdog_periodic();
    } while(r > 0);

    // Drop to some low power mode
    // lpm_drop();
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
