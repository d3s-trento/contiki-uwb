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

/**
 * \file
 *      Contiki DW1000 Driver Source File
 *
 * \author
 *      Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 *      Timofei Istomin <tim.ist@gmail.com>
 */

#include "dw1000.h"
#include "dw1000-arch.h"
#include "dw1000-ranging.h"
#include "dw1000-config.h"
#include "net/packetbuf.h"
#include "net/rime/rimestats.h"
#include "net/netstack.h"
#include "leds.h" /* To be removed after debugging */
#include <stdbool.h>
#include "dev/watchdog.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "deca_regs.h"
/*---------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while(0)
#endif

#define DEBUG_RNG_FAILED 0
#if DEBUG_RNG_FAILED
#include <stdio.h>
#define PRINTF_RNG_FAILED(...) printf(__VA_ARGS__)
#else
#define PRINTF_RNG_FAILED(...) do {} while(0)
#endif

#undef LEDS_TOGGLE
#if DW1000_DEBUG_LEDS
#define LEDS_TOGGLE(x) leds_toggle(x)
#else
#define LEDS_TOGGLE(x)
#endif
/*---------------------------------------------------------------------------*/
typedef enum {
  FRAME_RECEIVED = 1,
  RECV_TO,
  RECV_ERROR,
  TX_SUCCESS,
} dw1000_event_t;
/*---------------------------------------------------------------------------*/
/* Configuration constants */
/*#define DW1000_RX_AFTER_TX_DELAY 60 */
#define DW1000_RX_AFTER_TX_DELAY 0
/*---------------------------------------------------------------------------*/
/* Static variables */
#if DEBUG
static dw1000_event_t dw_dbg_event;
static uint32_t radio_status;
#endif
static uint16_t data_len; /* received data length (payload without CRC) */
static bool frame_pending;
static bool auto_ack_enabled;
static bool wait_ack_txdone;
static volatile bool tx_done; /* flag indicating the end of TX */
/*---------------------------------------------------------------------------*/
PROCESS(dw1000_process, "DW1000 driver");
#if DEBUG
PROCESS(dw1000_dbg_process, "DW1000 dbg process");
#endif
/*---------------------------------------------------------------------------*/
/* Declaration of static radio callback functions.
 * NOTE: For all events, corresponding interrupts are cleared and necessary
 * resets are performed. In addition, in the RXFCG case, received frame
 * information and frame control are read before calling the callback. If
 * double buffering is activated, it will also toggle between reception
 * buffers once the reception callback processing has ended.
 */
static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void tx_conf_cb(const dwt_cb_data_t *cb_data);
/*---------------------------------------------------------------------------*/
/* DW1000 Radio Driver Static Functions */
static int dw1000_init(void);
static int dw1000_prepare(const void *payload, unsigned short payload_len);
static int dw1000_transmit(unsigned short transmit_len);
static int dw1000_send(const void *payload, unsigned short payload_len);
static int dw1000_radio_read(void *buf, unsigned short buf_len);
static int dw1000_channel_clear(void);
static int dw1000_receiving_packet(void);
static int dw1000_pending_packet(void);
static int dw1000_on(void);
static int dw1000_off(void);
static radio_result_t dw1000_get_value(radio_param_t param, radio_value_t *value);
static radio_result_t dw1000_set_value(radio_param_t param, radio_value_t value);
static radio_result_t dw1000_get_object(radio_param_t param, void *dest, size_t size);
static radio_result_t dw1000_set_object(radio_param_t param, const void *src, size_t size);
/*---------------------------------------------------------------------------*/
/* Callback to process RX good frame events */
static void
rx_ok_cb(const dwt_cb_data_t *cb_data)
{
  /*LEDS_TOGGLE(LEDS_GREEN); */
#if DW1000_RANGING_ENABLED
  if(cb_data->rx_flags & DWT_CB_DATA_RX_FLAG_RNG) {
    dw1000_rng_ok_cb(cb_data);
    return;
  }
  /* got a non-ranging packet: reset the ranging module if */
  /* it was in the middle of ranging */
  dw1000_range_reset();
  PRINTF_RNG_FAILED("Err, non-rng.\n");
#endif

  data_len = cb_data->datalength - DW1000_CRC_LEN;
  /* Set the appropriate event flag */
  frame_pending = true;

  /* if we have auto-ACKs enabled and an ACK was requested, */
  /* don't signal the reception until the TX done interrupt */
  if(auto_ack_enabled && (cb_data->status & SYS_STATUS_AAT)) {
    /*leds_on(LEDS_ORANGE); */
    wait_ack_txdone = true;
  } else {
    wait_ack_txdone = false;
    process_poll(&dw1000_process);
  }
}
/*---------------------------------------------------------------------------*/
/* Callback to process RX timeout events */
static void
rx_to_cb(const dwt_cb_data_t *cb_data)
{
#if DW1000_RANGING_ENABLED
  dw1000_range_reset();
  PRINTF_RNG_FAILED("Err, to.\n");
#endif
#if DEBUG
  dw_dbg_event = RECV_TO;
  radio_status = cb_data->status;
  process_poll(&dw1000_dbg_process);
#endif
  dw1000_on();

  LEDS_TOGGLE(LEDS_YELLOW);
  /* Set LED PC7 */
}
/*---------------------------------------------------------------------------*/
/* Callback to process RX error events */
static void
rx_err_cb(const dwt_cb_data_t *cb_data)
{
#if DW1000_RANGING_ENABLED
  dw1000_range_reset();
  PRINTF_RNG_FAILED("Err, bad-rx.\n");
#endif
#if DEBUG
  dw_dbg_event = RECV_ERROR;
  radio_status = cb_data->status;
  process_poll(&dw1000_dbg_process);
#endif
  dw1000_on();

  /* Set LED PC8 */
  /*LEDS_TOGGLE(LEDS_RED); // not informative with frame filtering */
}
/* Callback to process TX confirmation events */
static void
tx_conf_cb(const dwt_cb_data_t *cb_data)
{
  /* Set LED PC9 */
  /*LEDS_TOGGLE(LEDS_ORANGE); */

  tx_done = 1; /* to stop waiting in dw1000_transmit() */

  /*if we are sending an auto ACK, signal the frame reception here */
  if(wait_ack_txdone) {
    wait_ack_txdone = false;
    process_poll(&dw1000_process);
  }
}
/*---------------------------------------------------------------------------*/

static int
dw1000_init(void)
{
  PRINTF("DW1000 driver init\n");

  /* Initialize arch-dependent DW1000 */
  dw1000_arch_init();

  /* Set the default configuration */
  dw1000_reset_cfg();

  /* Print the current configuration */
  dw1000_print_cfg();

  /* Configure DW1000 GPIOs to show TX/RX activity with the LEDs */
#if DW1000_DEBUG_LEDS
  dwt_setleds(DWT_LEDS_ENABLE);
#endif

  auto_ack_enabled = false;

#if DW1000_FRAMEFILTER == 1
  dw1000_set_value(RADIO_PARAM_RX_MODE, RADIO_RX_MODE_ADDRESS_FILTER);
#endif /* DW1000_FRAMEFILTER */

  /* Set the DW1000 ISR */
  dw1000_set_isr(dwt_isr);

  /* Register TX/RX callbacks. */
  dwt_setcallbacks(&tx_conf_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb);
  /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
  dwt_setinterrupt(DWT_INT_TFRS | DWT_INT_RFCG | DWT_INT_RFTO | DWT_INT_RXPTO |
                   DWT_INT_RPHE | DWT_INT_RFCE | DWT_INT_RFSL | DWT_INT_SFDT |
                   DWT_INT_ARFE, 1);

#if DW1000_RANGING_ENABLED
  dw1000_ranging_init();
#endif

  /* Configure deep sleep mode */
  /* NOTE: this is only used if the application actually calls the necessary
   * functions to put the radio in deep sleep mode and wake it up */
  dwt_configuresleep(DWT_PRESRV_SLEEP | DWT_CONFIG, DWT_WAKE_CS | DWT_SLP_EN);

  /* Start DW1000 process */
  process_start(&dw1000_process, NULL);
#if DEBUG
  process_start(&dw1000_dbg_process, NULL);
#endif /* DEBUG */

  return 0;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_prepare(const void *payload, unsigned short payload_len)
{
  uint8_t frame_len;

#if DW1000_RANGING_ENABLED
  if(dw1000_is_ranging()) {
    return 1;   /* error */
  }
#endif

  frame_len = payload_len + DW1000_CRC_LEN;

  /* Write frame data to DW1000 and prepare transmission */
  dwt_writetxdata(frame_len, (uint8_t *)payload, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(frame_len, 0, 0); /* Zero offset in TX buffer, no ranging. */
  /* TODO: check the return status of the operations above */
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_transmit(unsigned short transmit_len)
{
  int ret;
  int8_t irq_status = dw1000_disable_interrupt();
#if DW1000_RANGING_ENABLED
  if(dw1000_is_ranging()) {
    dw1000_enable_interrupt(irq_status);
    return RADIO_TX_ERR;
  }
#endif
  /* Switch off radio before setting it to transmit
   * It also clears pending interrupts */
  dwt_forcetrxoff();

  /* Radio starts listening certain delay (in UWB microseconds) after TX */
  dwt_setrxaftertxdelay(DW1000_RX_AFTER_TX_DELAY);

  tx_done = false;

  /* Start transmission, indicating that a response is expected so that reception
   * is enabled automatically after the frame is sent and the delay set by
   * dwt_setrxaftertxdelay() has elapsed. */
  ret = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
  dw1000_enable_interrupt(irq_status);

  if(ret != DWT_SUCCESS) {
    return RADIO_TX_ERR;
  }

  watchdog_periodic();
  while(!tx_done) {
    /* do nothing, could go to LPM mode */
    asm("");
    /* TODO: add a timeout */
  }
  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_send(const void *payload, unsigned short payload_len)
{
  if(0 == dw1000_prepare(payload, payload_len)) {
    return dw1000_transmit(payload_len);
  } else {
    return RADIO_TX_ERR;
  }
}
/*---------------------------------------------------------------------------*/
static int
dw1000_radio_read(void *buf, unsigned short buf_len)
{
  if(!frame_pending) {
    return 0;
  }
  dwt_readrxdata(buf, buf_len, 0);
  frame_pending = false;
  return buf_len;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_channel_clear(void)
{
#if DW1000_RANGING_ENABLED
  if(dw1000_is_ranging()) {
    return 0;
  }
#endif /* DW1000_RANGING_ENABLED */

  if(wait_ack_txdone) {
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_receiving_packet(void)
{
  /* TODO: fix this by checking the actual radio status */
  if(wait_ack_txdone) {
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_pending_packet(void)
{
  return frame_pending;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_on(void)
{
  /* Enable RX */
  dwt_setrxtimeout(0);
  dwt_rxenable(DWT_START_RX_IMMEDIATE);
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
dw1000_off(void)
{
  /* Turn off the transceiver */
  int8_t irq_status = dw1000_disable_interrupt();
#if DW1000_RANGING_ENABLED
  dw1000_range_reset(); /* In case we were ranging */
#endif
  dwt_forcetrxoff();
  dw1000_enable_interrupt(irq_status);

  return 0;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
dw1000_get_value(radio_param_t param, radio_value_t *value)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
dw1000_set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
  case RADIO_PARAM_PAN_ID:
    dwt_setpanid(value & 0xFFFF);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_16BIT_ADDR:
    dwt_setaddress16(value & 0xFFFF);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RX_MODE:
    if(value & RADIO_RX_MODE_ADDRESS_FILTER) {
      dwt_enableframefilter(DWT_FF_COORD_EN | DWT_FF_BEACON_EN | DWT_FF_DATA_EN | DWT_FF_ACK_EN | DWT_FF_MAC_EN);
#if DW1000_AUTOACK
      /* Auto-ack is only possible if frame filtering is activated */
      dwt_enableautoack(DW1000_AUTOACK_DELAY);
      auto_ack_enabled = true;
#endif
    } else {
      dwt_enableframefilter(DWT_FF_NOTYPE_EN);
      auto_ack_enabled = false;
    }
    return RADIO_RESULT_OK;
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
dw1000_get_object(radio_param_t param, void *dest, size_t size)
{
  if(param == RADIO_PARAM_64BIT_ADDR) {
    if(size != 8 || dest == NULL) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    uint8_t little_endian[8];
    int i;
    dwt_geteui(little_endian);

    for(i = 0; i < 8; i++) {
      ((uint8_t*)dest)[i] = little_endian[7 - i];
    }
    return RADIO_RESULT_OK;
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
dw1000_set_object(radio_param_t param, const void *src, size_t size)
{
  if(param == RADIO_PARAM_64BIT_ADDR) {
    if(size != 8 || src == NULL) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    uint8_t little_endian[8];
    int i;

    for(i = 0; i < 8; i++) {
      little_endian[i] = ((uint8_t *)src)[7 - i];
    }
    dwt_seteui(little_endian);

    return RADIO_RESULT_OK;
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(dw1000_process, ev, data)
{
  PROCESS_BEGIN();

  /*PRINTF("dw1000_process: started\n"); */

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    PRINTF("dwr frame\n");

    if(!frame_pending) {
      /* received a frame but it was already read (e.g. ACK) */
      /* re-enable rx */
      dw1000_on();
      continue;
    }

    if(data_len > PACKETBUF_SIZE) {
      frame_pending = false;
      dw1000_on();
      continue; /* packet is too big, drop it */
    }

    /* Clear packetbuf to avoid having leftovers from previous receptions */
    packetbuf_clear();

    /* Copy the received frame to packetbuf */
    dw1000_radio_read(packetbuf_dataptr(), data_len);
    packetbuf_set_datalen(data_len);

    /* Re-enable RX to keep listening */
    dw1000_on();
    /*PRINTF("dw1000_process: calling recv cb, len %d\n", data_len); */
    NETSTACK_RDC.input();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
const struct radio_driver dw1000_driver =
{
  dw1000_init,
  dw1000_prepare,
  dw1000_transmit,
  dw1000_send,
  dw1000_radio_read,
  dw1000_channel_clear,
  dw1000_receiving_packet,
  dw1000_pending_packet,
  dw1000_on,
  dw1000_off,
  dw1000_get_value,
  dw1000_set_value,
  dw1000_get_object,
  dw1000_set_object
};
/*---------------------------------------------------------------------------*/
/* Functions to put DW1000 into deep sleep mode and wake it up */
void
dw1000_sleep(void)
{
  dwt_entersleep();
}
/*---------------------------------------------------------------------------*/
#define WAKEUP_BUFFER_LEN 600
static uint8_t wakeup_buffer[WAKEUP_BUFFER_LEN];
/*---------------------------------------------------------------------------*/
int
dw1000_wakeup(void)
{
  return dwt_spicswakeup(wakeup_buffer, WAKEUP_BUFFER_LEN);
}
/*---------------------------------------------------------------------------*/
bool
range_with(linkaddr_t *dst, dw1000_rng_type_t type)
{
#if DW1000_RANGING_ENABLED
  return dw1000_range_with(dst, type);
#else
  return false;
#endif
}
#if DEBUG
PROCESS_THREAD(dw1000_dbg_process, ev, data)
{
  static struct etimer et;
  static uint32_t r1;
  static uint8_t r2;
  PROCESS_BEGIN();
  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_POLL) {
      r1 = radio_status;
      printf("RX ERR(%u) %02x %02x %02x %02x\n",
             dw_dbg_event, (uint8_t)(r1 >> 24), (uint8_t)(r1 >> 16),
             (uint8_t)(r1 >> 8), (uint8_t)r1);
    }
    if(etimer_expired(&et)) {
      r1 = dwt_read32bitoffsetreg(SYS_STATUS_ID, 0);
      r2 = dwt_read8bitoffsetreg(SYS_STATUS_ID, 4);
      printf("*** SYS_STATUS %02x %02x %02x %02x %02x ***\n",
             (uint8_t)(r1 >> 24), (uint8_t)(r1 >> 16), (uint8_t)(r1 >> 8),
             (uint8_t)(r1), r2);
      dw_dbg_event = 0;
    }
  }
  PROCESS_END();
}
#endif /* DEBUG */
