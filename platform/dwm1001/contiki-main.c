/*
 * Copyright (c) 2015, Nordic Semiconductor
 * Copyright (c) 2018, University of Trento, Italy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup nrf52dk nRF52 Development Kit
 * @{
 */
#include <stdio.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "custom_board.h"
#include "dwm1001-dev-board.h"
#include "leds.h"
#include "lib/sensors.h"
/*---------------------------------------------------------------------------*/
#include "nordic_common.h"
#include "nrfx_gpiote.h"
/*---------------------------------------------------------------------------*/
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
/*---------------------------------------------------------------------------*/
#include "dev/watchdog.h"
#include "dev/serial-line.h"
#include "dev/uart0.h"
#include "dev/lpm.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "dw1000-arch.h"
#include "dw1000-config.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else //DEBUG
#define PRINTF(...)
#endif //DEBUG

/*---------------------------------------------------------------------------*/
#if NETSTACK_CONF_WITH_IPV6
#include "uip-debug.h"
#include "net/ipv6/uip-ds6.h"
#endif //NETSTACK_CONF_WITH_IPV6

/*---------------------------------------------------------------------------*/
/**
 * \brief Board specific initialization
 *
 * This function will enable SoftDevice is present.
 */
static void
board_init(void)
{
  // needed for button and for the DW1000 interrupt
  if (!nrfx_gpiote_is_init()) {
    nrfx_gpiote_init();
  }
}
/*---------------------------------------------------------------------------*/
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

#if NETSTACK_RADIO == dw1000_driver

  NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, IEEE802154_PANID);
  NETSTACK_RADIO.set_object(RADIO_PARAM_64BIT_ADDR, ext_addr, 8);
  NETSTACK_RADIO.set_value(RADIO_PARAM_16BIT_ADDR,
			   (ext_addr[6]) << 8 | (ext_addr[7])); // converting from big-endian format


#else /* NETSTACK_RADIO == dw1000_driver */
#error NESTACK RADIO is not UWB

  /* Set up the IEEE 802.15.4 PANID, short and long address
   * to be able to enable HW frame filtering
   * NOTE:
   * We don't use the NETSTACK_RADIO functions as we may be using
   * the BLE stack
   */
  dwt_setpanid(IEEE802154_PANID & 0xFFFF);
  dwt_setaddress16((ext_addr[6]) << 8 | (ext_addr[7])); // converting from big-endian format
  uint8_t little_endian[8];
  int i;

  for(i = 0; i < 8; i++) {
    little_endian[i] = ((uint8_t *)ext_addr)[7 - i];
  }
  dwt_seteui(little_endian);
#endif /* NETSTACK_RADIO == dw1000_driver */

}
/*---------------------------------------------------------------------------*/
/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
#if defined(NRF_LOG_ENABLED) && NRF_LOG_ENABLED == 1
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif //defined(NRF_LOG_ENABLED) && NRF_LOG_ENABLED == 1
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Main function for nRF52dk platform.
 * \note This function doesn't return.
 */
int
main(void)
{
#ifdef NRF_SHOW_RESETREASON
  uint32_t resetreas = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = 0xffffffff; /* to clear reset reason*/
#endif //NRF_SHOW_RESETREASON

  board_init();

  /* Enable the FPU */
    __asm volatile
    (
     "LDR.W R0, =0xE000ED88             \n"
     "LDR R1, [R0]                      \n"
     "ORR R1, R1, #(0xF << 20)          \n"
     "STR R1, [R0]                      \n"
     "DSB                               \n"
     "ISB                               \n"
    );

  log_init();
  leds_init();

  clock_init();
  rtimer_init();

  watchdog_init();
  process_init();

  /* Seed value is ignored since hardware RNG is used. */
  random_init(0);

#if defined(UART0_ENABLED) && UART0_ENABLED == 1
  uart0_init();
#if SLIP_ARCH_CONF_ENABLE
  slip_arch_init(0);
#else
  uart0_set_input(serial_line_input_byte);
  serial_line_init();
#endif
#endif

  PRINTF("Starting " CONTIKI_VERSION_STRING "\n");
#ifdef NRF_SHOW_RESETREASON
  PRINTF("Reset reason %08lx %s%s%s%s%s%s%s%s*\n", resetreas,
	 ((resetreas & POWER_RESETREAS_NFC_Msk) == POWER_RESETREAS_NFC_Msk) ? "NFC," :  "",
	 ((resetreas & POWER_RESETREAS_DIF_Msk) == POWER_RESETREAS_DIF_Msk) ? "DIF," :  "",
	 ((resetreas & POWER_RESETREAS_LPCOMP_Msk) == POWER_RESETREAS_LPCOMP_Msk) ? "LPCOMP," :  "",
	 ((resetreas & POWER_RESETREAS_OFF_Msk) == POWER_RESETREAS_OFF_Msk) ? "OFF," :  "",
	 ((resetreas & POWER_RESETREAS_LOCKUP_Msk) == POWER_RESETREAS_LOCKUP_Msk) ? "LOCKUP," :  "",
	 ((resetreas & POWER_RESETREAS_SREQ_Msk) == POWER_RESETREAS_SREQ_Msk) ? "SREQ," :  "",
	 ((resetreas & POWER_RESETREAS_DOG_Msk) == POWER_RESETREAS_DOG_Msk) ? "DOG," :  "",
	 ((resetreas & POWER_RESETREAS_RESETPIN_Msk) == POWER_RESETREAS_RESETPIN_Msk) ? "RESETPIN," :  ""
	 );
#endif //NRF_SHOW_RESETREASON

#if NETSTACK_RADIO == dw1000_driver
#if NETSTACK_NETWORK == rime_driver
  PRINTF("Network stack: Rime over UWB\n");
#elif NETSTACK_NETWORK == sicslowpan_driver
  PRINTF("Network stack: IPv6 over UWB\n");
#else
#error "NETSTACK on UWB but NETWORK layer is not set"
#endif /* NETSTACK_NETWORK valuese*/
#else /*NETSTACK_RADIO == dw1000_driver*/
#error "NETSTACK_RADIO isn't UWB"
#endif /*NETSTACK_RADIO == dw1000_driver*/

  process_start(&etimer_process, NULL);
  ctimer_init();

#if ENERGEST_CONF_ON
  energest_init();
  ENERGEST_ON(ENERGEST_TYPE_CPU);
#endif

#if NETSTACK_RADIO == dw1000_driver
  netstack_init(); /* Init the full UWB network stack */

  configure_addresses(); /* Set the link layer addresses and the PAN ID */

#if defined(NETSTACK_CONF_WITH_IPV6) && NETSTACK_CONF_WITH_IPV6==1
  memcpy(&uip_lladdr.addr, &linkaddr_node_addr, sizeof(uip_lladdr.addr));
  queuebuf_init();
  process_start(&tcpip_process, NULL);
#endif /* NETSTACK_CONF_NETWORK == sicslowpan_driver //IPV6 */

  printf("Short address: 0x%02x%02x\n",
	 linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

#else  /*  NETSTACK_CONF_RADIO == dw1000_driver */
#error "NETSTACK is not on UWB"
  /* NETSTACK is not over DW1000, it should be initializd in nay case*/
  dw1000_arch_init(); /* Only initialize the radio hardware, not the network stack */
  dw1000_reset_cfg(); /* and set the default configuration */

#endif /*  NETSTACK_CONF_RADIO == dw1000_driver */

  process_start(&sensors_process, NULL);
  autostart_start(autostart_processes);

  watchdog_start();

  while(1) {
    uint8_t r;
    do {
      r = process_run();
      watchdog_periodic();
    } while(r > 0);

    lpm_drop();
  }
}
/**
 * @}
 */
