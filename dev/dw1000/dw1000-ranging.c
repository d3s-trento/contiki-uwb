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
 *      Contiki DW1000 Driver ranging module
 *
 * \author
 *      Timofei Istomin <tim.ist@gmail.com>
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "core/net/linkaddr.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "leds.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "deca_regs.h"
#include "dw1000-ranging.h"
#include "dw1000-config.h"
#include "dw1000-cir.h"
#include "process.h"
#include "deca_range_tables.h"
/*---------------------------------------------------------------------------*/
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#if DW1000_RANGING_ENABLED

#if LINKADDR_SIZE > 2
#warning The ranging module will use 16-bit addresses. Make sure they are unique in your deployment!
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while(0)
#endif

#ifndef DEBUG_RNG
#define DEBUG_RNG 0
#endif

#if DEBUG_RNG
#include <stdio.h>
#define PRINTF_RNG(...) printf(__VA_ARGS__)
#else
#define PRINTF_RNG(...) do {} while(0)
#endif

#define DEBUG_RNG_FAILED 0
#if DEBUG_RNG_FAILED
#include <stdio.h>
#define PRINTF_RNG_FAILED(...) printf(__VA_ARGS__)
#else
#define PRINTF_RNG_FAILED(...) do {} while(0)
#endif

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

process_event_t ranging_event;
struct process *req_process;
static ranging_data_t ranging_data;
static int status;
static dw1000_rng_type_t rng_type;
static bool print_cir_requested;

typedef enum {
  S_WAIT_POLL, /* 0 */
  S_WAIT_SS1,  /* 1 */
  S_WAIT_DS1,  /* 2 */
  S_WAIT_DS2,  /* 3 */
  S_WAIT_DS3,  /* 4 */
  S_RANGING_DONE,      /* 5 */
  S_RANGING_DONE_MSG4, /* 6 */
  S_ABORT,     /* 7 */
  S_RESET      /* 8 */
} state_t;

static state_t state;
static state_t old_state;

/* Packet lengths for different messages (include the 2-byte CRC!) */
#define PKT_LEN_POLL 12

#define PKT_LEN_SS1 20

#define PKT_LEN_DS1 12
#define PKT_LEN_DS2 24
#define PKT_LEN_DS3 24

/* Packet types for different messages */
#define MSG_TYPE_SS0 0xE0
#define MSG_TYPE_SS1 0xE1

#define MSG_TYPE_DS0 0xD0
#define MSG_TYPE_DS1 0xD1
#define MSG_TYPE_DS2 0xD2
#define MSG_TYPE_DS3 0xD3

/* Indexes to access some of the fields in the frames defined above. */
#define IDX_SN 2
#define IDX_TYPE 9

/*SS*/
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14

/*DS*/
#define FINAL_MSG_POLL_TX_TS_IDX 10
#define FINAL_MSG_RESP_RX_TS_IDX 14
#define FINAL_MSG_FINAL_TX_TS_IDX 18

#define DISTANCE_MSG_POLL_RX_IDX 10
#define DISTANCE_MSG_RESP_TX_IDX 14
#define DISTANCE_MSG_FINAL_RX_IDX 18

static uint8_t my_seqn;
static uint8_t recv_seqn;
static linkaddr_t ranging_with; /* the current ranging peer */

#define RX_BUF_LEN 24
static uint8_t tx_buf[RX_BUF_LEN];
static uint8_t rx_buf[RX_BUF_LEN];

typedef struct {
  /* SS and DS timeouts */
  uint32_t a;
  uint32_t rx_dly_a;
  uint16_t to_a;

/* DS timeouts */
  uint32_t rx_dly_b;
  uint32_t b;
  uint16_t to_b;
  uint16_t to_c;

  uint32_t finish_delay; /* assumes millisecond clock tick! */

  double freq_offs_multiplier;
} ranging_conf_t;

/*
*              DS0/SS0 --------->
* timeout: to_a                  tx: rx_ts + a
*              <--------- DS1/SS1
* tx: rx_ts + b                  timeout: to_b
*              DS2 ------------->
* timeout: to_c                  tx: immediate
*              <------------- DS3
*/

/* The following are tuned for 128us preamble */
const static ranging_conf_t ranging_conf_6M8 = {
/* SS and DS timeouts */
  .a = 500,           // evb1000 can do 400
  .rx_dly_a = 100,    // timeout starts after this
  .to_a = 500,        // evb1000 can do 400

/* DS timeouts */
  .b = 550,           // evb1000 can do 400
  .rx_dly_b = 100,    // timeout starts after this
  .to_b = 550,        // evb1000 can do 400
  .to_c = 550,        // evb1000 can do 400

  .finish_delay = 1, /* assumes millisecond clock tick! */

  .freq_offs_multiplier = FREQ_OFFSET_MULTIPLIER,
};

const static ranging_conf_t ranging_conf_110K = {
/* SS and DS timeouts */
  .a = 3000,
  .rx_dly_a = 0,
  .to_a = 4000,

/* DS timeouts */
  .b = 3000,
  .rx_dly_b = 0,
  .to_b = 4500,
  .to_c = 3500, /* 3000 kind of works, too */

  .finish_delay = 3, /* assumes millisecond clock tick! */

  .freq_offs_multiplier = FREQ_OFFSET_MULTIPLIER_110KB,
};

static uint16_t rx_ant_dly;
static uint16_t tx_ant_dly;
static int channel;
static double hertz_to_ppm_multiplier;

static ranging_conf_t ranging_conf;
/*---------------------------------------------------------------------------*/
#define tx_buf_set_src() do { tx_buf[7] = linkaddr_node_addr.u8[LINKADDR_SIZE-1]; tx_buf[8] = linkaddr_node_addr.u8[LINKADDR_SIZE-2]; } while(0)
#define tx_buf_set_dst() do { tx_buf[5] = ranging_with.u8[LINKADDR_SIZE-1]; tx_buf[6] = ranging_with.u8[LINKADDR_SIZE-2]; } while(0)
#define tx_buf_set_dst_from_src() do { tx_buf[5] = rx_buf[7]; tx_buf[6] = rx_buf[8]; } while(0)
#define rx_buf_check_dst() (rx_buf[5] == linkaddr_node_addr.u8[LINKADDR_SIZE-1] && rx_buf[6] == linkaddr_node_addr.u8[LINKADDR_SIZE-2]) /* TODO: check also PANID */
#define rx_buf_check_src() (rx_buf[7] == ranging_with.u8[LINKADDR_SIZE-1] && rx_buf[8] == ranging_with.u8[LINKADDR_SIZE-2])
/*---------------------------------------------------------------------------*/
static inline uint64_t
get_rx_timestamp_u64(void)
{
  uint8_t ts_tab[5];
  uint64_t ts = 0;
  int i;
  dwt_readrxtimestamp(ts_tab);
  for(i = 4; i >= 0; i--) {
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}
/*---------------------------------------------------------------------------*/
static inline uint64_t
get_tx_timestamp_u64(void)
{
  uint8_t ts_tab[5];
  uint64_t ts = 0;
  int i;
  dwt_readtxtimestamp(ts_tab);
  for(i = 4; i >= 0; i--) {
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}
/*---------------------------------------------------------------------------*/
static inline void
msg_get_ts(uint8_t *ts_field, uint32_t *ts)
{
  int i;
  *ts = 0;
  for(i = 0; i < 4; i++) {
    *ts |= ts_field[i] << (i * 8);
  }
}
/*---------------------------------------------------------------------------*/
static inline void
msg_set_ts(uint8_t *ts_field, uint64_t ts)
{
  int i;
  for(i = 0; i < 4; i++) {
    ts_field[i] = (uint8_t)ts;
    ts >>= 8;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(dw1000_rng_process, "DW1000 dbg process");
#if DEBUG
PROCESS(dw1000_rng_dbg_process, "DW1000 rng dbg process");
#endif
/*---------------------------------------------------------------------------*/

/* (Re)initialise the ranging module.
 *
 * Needs to be called before issuing or serving ranging requests and
 * after changing radio parameters. */
void
dw1000_ranging_init()
{
  const dwt_config_t * cfg;
  cfg = dw1000_get_current_cfg();
  dw1000_get_current_ant_dly(&rx_ant_dly, &tx_ant_dly);
  channel = cfg->chan;

  switch(cfg->dataRate) {
  case DWT_BR_6M8:
    ranging_conf = ranging_conf_6M8;
    break;

  case DWT_BR_110K:
    ranging_conf = ranging_conf_110K;
    break;
  }

  switch (channel) {
    case 1: hertz_to_ppm_multiplier = HERTZ_TO_PPM_MULTIPLIER_CHAN_1; break;
    case 2:
    case 4: hertz_to_ppm_multiplier = HERTZ_TO_PPM_MULTIPLIER_CHAN_2; break;
    case 3: hertz_to_ppm_multiplier = HERTZ_TO_PPM_MULTIPLIER_CHAN_3; break;
    case 5:
    case 7: hertz_to_ppm_multiplier = HERTZ_TO_PPM_MULTIPLIER_CHAN_3; break;
  }

  /* Fill in the constant part of the TX buffer */
  tx_buf[0] = 0x41;
  tx_buf[1] = 0x88;
  tx_buf[3] = IEEE802154_PANID & 0xff;
  tx_buf[4] = IEEE802154_PANID >> 8;

  process_start(&dw1000_rng_process, NULL);
  state = S_WAIT_POLL;
}
/*---------------------------------------------------------------------------*/
/* XXX now it works only with 2-byte addresses */
bool
dw1000_range_with(linkaddr_t *lladdr, dw1000_rng_type_t type)
{
  int8_t irq_status;
  bool ret;
  if(type != DW1000_RNG_SS && type != DW1000_RNG_DS) {
    return false;
  }

  if(!ranging_event) {
    return false; /* first call the init function */
  }
  if(req_process != PROCESS_NONE) {
    return false; /* already ranging */
  }
  irq_status = dw1000_disable_interrupt();

  if(state != S_WAIT_POLL) {
    ret = false;
    goto enable_interrupts;
  }

  dwt_forcetrxoff();

  ranging_with = *lladdr;
  ranging_data.status = 0;
  ranging_data.distance = 0;
  ranging_data.raw_distance = 0;
  rng_type = type;

  my_seqn++;

  PRINTF_RNG("dwr: rng start %d type %d\n", my_seqn, rng_type);

  /* Write frame data to DW1000 and prepare transmission. */
  tx_buf[IDX_SN] = my_seqn;
  tx_buf[IDX_TYPE] = (rng_type == DW1000_RNG_SS) ? MSG_TYPE_SS0 : MSG_TYPE_DS0;
  tx_buf_set_dst();
  tx_buf_set_src();

  /* Set expected response's delay and timeout.*/
  dwt_setrxaftertxdelay(ranging_conf.rx_dly_a);
  dwt_setrxtimeout(ranging_conf.to_a);

  /* Write frame data to DW1000 and prepare transmission. */
  /* dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS); */
  dwt_writetxdata(PKT_LEN_POLL, tx_buf, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(PKT_LEN_POLL, 0, 1); /* Zero offset in TX buffer, ranging. */

  /* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
   * set by dwt_setrxaftertxdelay() has elapsed. */

  if(dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) != DWT_SUCCESS) {
    ret = false;
    PRINTF_RNG_FAILED("dwr: error in tx of rframe.\n");
    goto enable_interrupts;
  }

  ret = true; /* the request is successful */
  state = (rng_type == DW1000_RNG_SS) ? S_WAIT_SS1 : S_WAIT_DS1;
  req_process = PROCESS_CURRENT();

enable_interrupts:
  dw1000_enable_interrupt(irq_status);
  PRINTF_RNG("dwr3: %d\n", my_seqn);
  return ret;
}
/*---------------------------------------------------------------------------*/
/* Timestamps needed for SS computations */
uint32_t ss_poll_tx_ts, ss_resp_rx_ts, ss_poll_rx_ts, ss_resp_tx_ts;
/* Timestamps needed for DS computations */
uint32_t ds_poll_tx_ts, ds_resp_rx_ts, ds_final_tx_ts;
uint32_t ds_poll_rx_ts, ds_resp_tx_ts, ds_final_rx_ts;

// clock frequency offset to compensate the distance bias in SS-TWR
float clockOffsetRatio;

/*---------------------------------------------------------------------------*/
/* Callback to process ranging good frame events
 */
void
dw1000_rng_ok_cb(const dwt_cb_data_t *cb_data)
{
  uint16_t pkt_len = cb_data->datalength;

  /* if(! (cb_data->rx_flags & DWT_CB_DATA_RX_FLAG_RNG)) {
   *  goto abort; // got a non-ranging packet, abort the ranging session
   * } */

  if(state == S_WAIT_POLL) {
    if(pkt_len != PKT_LEN_POLL) {
      status = 11;
      goto abort;
    }

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    if(!rx_buf_check_dst()) {
      status = 12;
      goto abort;
    }
    if(rx_buf[IDX_TYPE] == MSG_TYPE_SS0) {  /* --- Single-sided poll --- */

      /* Timestamps of frames transmission/reception.
       * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
      uint64_t poll_rx_ts_64;
      uint64_t resp_tx_ts_64;
      uint32_t resp_tx_time;

      PRINTF_RNG("dwr: got SS0.\n");
      /* Retrieve poll reception timestamp. */
      poll_rx_ts_64 = get_rx_timestamp_u64();

      /* Compute final message transmission time. */
      resp_tx_time = (poll_rx_ts_64 + (ranging_conf.a * UUS_TO_DWT_TIME)) >> 8;
      dwt_setdelayedtrxtime(resp_tx_time);

      /* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
      resp_tx_ts_64 = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + tx_ant_dly;

      /* Write all timestamps in the final message. */
      msg_set_ts(&tx_buf[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts_64);
      msg_set_ts(&tx_buf[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts_64);

      /* Write and send the response message. */
      recv_seqn = rx_buf[IDX_SN];
      tx_buf[IDX_SN] = rx_buf[IDX_SN];
      tx_buf[IDX_TYPE] = MSG_TYPE_SS1;
      tx_buf_set_src();
      tx_buf_set_dst_from_src();

      dwt_writetxdata(PKT_LEN_SS1, tx_buf, 0); /* Zero offset in TX buffer. */
      dwt_writetxfctrl(PKT_LEN_SS1, 0, 1); /* Zero offset in TX buffer, ranging. */
      dwt_setrxaftertxdelay(0);
      dwt_setrxtimeout(0);
      dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
      int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
      if(ret != DWT_SUCCESS) {
        status = 14;
        PRINTF_RNG_FAILED("dwr: error in tx of SS1.\n");
        goto abort;
      }
      return; /* In this case, no other processing nor state change are needed */
    } else if(rx_buf[IDX_TYPE] == MSG_TYPE_DS0) { /* --- Double-sided poll --- */
      uint32_t resp_tx_time;
      uint64_t poll_rx_ts_64;

      PRINTF_RNG("dwr: got DS0.\n");

      /* Retrieve poll and store poll reception timestamp. */
      poll_rx_ts_64 = get_rx_timestamp_u64();
      ds_poll_rx_ts = (uint32_t)poll_rx_ts_64;

      /* Set send time for response. */
      resp_tx_time = (poll_rx_ts_64 + (ranging_conf.a * UUS_TO_DWT_TIME)) >> 8;
      dwt_setdelayedtrxtime(resp_tx_time);

      /* Set expected delay and timeout for final message reception. */
      dwt_setrxaftertxdelay(ranging_conf.rx_dly_b);
      dwt_setrxtimeout(ranging_conf.to_b);

      /* Write and send the response message. */
      recv_seqn = rx_buf[IDX_SN];
      tx_buf[IDX_SN] = rx_buf[IDX_SN];
      tx_buf[IDX_TYPE] = MSG_TYPE_DS1;
      tx_buf_set_src();
      tx_buf_set_dst_from_src();
      ranging_with.u8[LINKADDR_SIZE-1] = rx_buf[7];
      ranging_with.u8[LINKADDR_SIZE-2] = rx_buf[8];

      dwt_writetxdata(PKT_LEN_DS1, tx_buf, 0); /* Zero offset in TX buffer. */
      dwt_writetxfctrl(PKT_LEN_DS1, 0, 1); /* Zero offset in TX buffer, ranging. */
      dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
      int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
      if(ret != DWT_SUCCESS) {
        status = 15;
        PRINTF_RNG_FAILED("dwr: error in tx of DS1.\n");
        goto abort;
      }

      old_state = state;
      state = S_WAIT_DS2;
      return; /* done with this packet */
    } else {
      status = 13;
      goto abort;
    }
  } else if(state == S_WAIT_SS1) { /* --- We are waiting for the SS1 response ---- */
    if(pkt_len != PKT_LEN_SS1) {
      status = 1;
      goto abort;
    }

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    if(!rx_buf_check_dst()) {
      status = 2;
      goto abort;
    }
    if(!rx_buf_check_src()) {
      status = 3;
      goto abort; /* got reply from a wrong node */
    }

    /* Check that the frame is the expected response */
    if(rx_buf[IDX_TYPE] != MSG_TYPE_SS1) {
      status = 4;
      goto abort;
    }

    /* Retrieve poll transmission and response reception timestamps. */
    ss_poll_tx_ts = dwt_readtxtimestamplo32();
    ss_resp_rx_ts = dwt_readrxtimestamplo32();

    /* Read carrier integrator value and calculate clock offset ratio. */
    clockOffsetRatio = dwt_readcarrierintegrator() * (ranging_conf.freq_offs_multiplier * hertz_to_ppm_multiplier / 1.0e6);

    /* Get timestamps embedded in response message. */
    msg_get_ts(&rx_buf[RESP_MSG_POLL_RX_TS_IDX], &ss_poll_rx_ts);
    msg_get_ts(&rx_buf[RESP_MSG_RESP_TX_TS_IDX], &ss_resp_tx_ts);

    old_state = state;
    state = S_RANGING_DONE;

    goto poll_the_process;
  } else if(state == S_WAIT_DS1) { /* --- We are waiting for the DS1 response --- */
    if(pkt_len != PKT_LEN_DS1) {
      status = 41;
      goto abort;
    }

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    if(!rx_buf_check_dst()) {
      status = 42;
      goto abort;
    }
    if(!rx_buf_check_src()) {
      status = 43;
      goto abort; /* got reply from a wrong node */
    }

    /* Check that the frame is the expected response */
    if(rx_buf[IDX_TYPE] != MSG_TYPE_DS1) {
      status = 44;
      goto abort;
    }

    PRINTF_RNG("dwr: got DS1.\n");

    uint64_t final_tx_ts_64;
    uint32_t final_tx_time;
    uint64_t poll_tx_ts_64, resp_rx_ts_64;

    /* Retrieve poll transmission and response reception timestamp. */
    poll_tx_ts_64 = get_tx_timestamp_u64();
    resp_rx_ts_64 = get_rx_timestamp_u64();

    tx_buf[IDX_TYPE] = MSG_TYPE_DS2;

    /* Compute final message transmission time. */
    final_tx_time = (resp_rx_ts_64 + (ranging_conf.b * UUS_TO_DWT_TIME)) >> 8;
    dwt_setdelayedtrxtime(final_tx_time);

    dwt_setrxaftertxdelay(0);
    dwt_setrxtimeout(ranging_conf.to_c);

    /* Final TX timestamp is the transmission time we programmed plus the TX antenna delay. */
    final_tx_ts_64 = (((uint64_t)(final_tx_time & 0xFFFFFFFEUL)) << 8) + tx_ant_dly;

    ds_poll_tx_ts = (uint32_t)poll_tx_ts_64;
    ds_resp_rx_ts = (uint32_t)resp_rx_ts_64;
    ds_final_tx_ts = (uint32_t)final_tx_ts_64;

    /* Write all timestamps in the final message. */
    msg_set_ts(&tx_buf[FINAL_MSG_POLL_TX_TS_IDX], ds_poll_tx_ts);
    msg_set_ts(&tx_buf[FINAL_MSG_RESP_RX_TS_IDX], ds_resp_rx_ts);
    msg_set_ts(&tx_buf[FINAL_MSG_FINAL_TX_TS_IDX], ds_final_tx_ts);

    dwt_writetxdata(PKT_LEN_DS2, tx_buf, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(PKT_LEN_DS2, 0, 1); /* Zero offset in TX buffer, ranging. */
    dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
    int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);

    if(ret != DWT_SUCCESS) {
      status = 45;
      PRINTF_RNG_FAILED("dwr: error in tx of DS2.\n");
      goto abort;
    }

    old_state = state;
    state = S_WAIT_DS3;
    return;
  } else if(state == S_WAIT_DS2) { /* --- We are waiting for the DS2 response --- */
    if(pkt_len != PKT_LEN_DS2) {
      status = 51;
      goto abort;
    }

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    if(!rx_buf_check_dst()) {
      status = 52;
      goto abort;
    }
    if(!rx_buf_check_src()) {
      status = 53;
      goto abort; /* got reply from a wrong node */
    }

    /* Check that the frame is the expected response */
    if(rx_buf[IDX_TYPE] != MSG_TYPE_DS2) {
      status = 54;
      goto abort;
    }

    PRINTF_RNG("dwr: got DS2.\n");

    //dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    /* ds_poll_rx_ts was stored on the previous step */
    /* Retrieve response transmission and final reception timestamps. */
    ds_resp_tx_ts = (uint32_t)get_tx_timestamp_u64();
    ds_final_rx_ts = (uint32_t)get_rx_timestamp_u64();

    /* Get timestamps embedded in the final message. */
    msg_get_ts(&rx_buf[FINAL_MSG_POLL_TX_TS_IDX], &ds_poll_tx_ts);
    msg_get_ts(&rx_buf[FINAL_MSG_RESP_RX_TS_IDX], &ds_resp_rx_ts);
    msg_get_ts(&rx_buf[FINAL_MSG_FINAL_TX_TS_IDX], &ds_final_tx_ts);

    recv_seqn = rx_buf[IDX_SN];
    tx_buf[IDX_SN] = rx_buf[IDX_SN];
    tx_buf[IDX_TYPE] = MSG_TYPE_DS3;
    tx_buf_set_src();
    tx_buf_set_dst_from_src();

    /* Reply with timestamps */
    memcpy(&tx_buf[DISTANCE_MSG_POLL_RX_IDX], &ds_poll_rx_ts, 4);
    memcpy(&tx_buf[DISTANCE_MSG_RESP_TX_IDX], &ds_resp_tx_ts, 4);
    memcpy(&tx_buf[DISTANCE_MSG_FINAL_RX_IDX], &ds_final_rx_ts, 4);

    dwt_writetxdata(PKT_LEN_DS3, tx_buf, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(PKT_LEN_DS3, 0, 1); /* Zero offset in TX buffer, ranging. */
    dwt_setrxaftertxdelay(0);
    dwt_setrxtimeout(0);
    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    PRINTF("dwr: sent DS3.\n");

    /* done with this ranging session but still sending the 4th message */
    old_state = state;
    state = S_RANGING_DONE_MSG4;

    goto poll_the_process;
  } else if(state == S_WAIT_DS3) { /* --- We are waiting for the DS3 response --- */
    if(pkt_len != PKT_LEN_DS3) {
      status = 61;
      goto abort;
    }

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    if(!rx_buf_check_dst()) {
      status = 62;
      goto abort;
    }
    if(!rx_buf_check_src()) {
      status = 63;
      goto abort; /* got reply from a wrong node */
    }

    /* Check that the frame is the expected response */
    if(rx_buf[IDX_TYPE] != MSG_TYPE_DS3) {
      status = 64;
      goto abort;
    }

    PRINTF("dwr: got DS3.\n");

    dwt_readrxdata(rx_buf, pkt_len - DW1000_CRC_LEN, 0);

    msg_get_ts(&rx_buf[DISTANCE_MSG_POLL_RX_IDX], &ds_poll_rx_ts);
    msg_get_ts(&rx_buf[DISTANCE_MSG_RESP_TX_IDX], &ds_resp_tx_ts);
    msg_get_ts(&rx_buf[DISTANCE_MSG_FINAL_RX_IDX], &ds_final_rx_ts);

    old_state = state;
    state = S_RANGING_DONE;
    goto poll_the_process;
  }

abort: /* In case we got anything unexpected */
  old_state = state;
  state = S_ABORT;
  dwt_forcetrxoff();
  dwt_rxreset(); /* just to check */
poll_the_process:
  process_poll(&dw1000_rng_process);
}
/*---------------------------------------------------------------------------*/
static double
ss_tof_calc()
{
  int32_t rtd_init, rtd_resp;
  /* Compute time of flight. */
  rtd_init = ss_resp_rx_ts - ss_poll_tx_ts;
  rtd_resp = ss_resp_tx_ts - ss_poll_rx_ts;

  return ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS; // TODO with clock drift compensation
  //return ((rtd_init - rtd_resp) / 2.0) * DWT_TIME_UNITS; // without the compensation
}
/*---------------------------------------------------------------------------*/
static double
ds_tof_calc()
{
  double Ra, Rb, Da, Db;
  int64_t tof_dtu;

  /* Compute time of flight.
   * 32-bit subtractions give correct answers even if clock has wrapped. */
  Ra = (double)(ds_resp_rx_ts - ds_poll_tx_ts);
  Rb = (double)(ds_final_rx_ts - ds_resp_tx_ts);
  Da = (double)(ds_final_tx_ts - ds_resp_rx_ts);
  Db = (double)(ds_resp_tx_ts - ds_poll_rx_ts);
  tof_dtu = (int64_t)((Ra * Rb - Da * Db) / (Ra + Rb + Da + Db));

  return (double)(tof_dtu * DWT_TIME_UNITS);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(dw1000_rng_process, ev, data)
{
  static struct etimer abort_timer;
  PROCESS_BEGIN();

  if(!ranging_event) {
    ranging_event = process_alloc_event();
  }

#if DEBUG
  process_start(&dw1000_rng_dbg_process, NULL);
#endif

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    if (state == S_RANGING_DONE && print_cir_requested) {
      print_cir();
    }
    if (state != S_RANGING_DONE_MSG4) {
      // re-enable the RX unless we are sending the DS3 in which case
      // the radio will be enabled automatically.
      dwt_setrxtimeout(0);
      dwt_rxenable(0);
    }
    PRINTF_RNG("dwr: process: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, status);
#if PRINTF_RNG == 0
    if(state == S_RESET || state == S_ABORT) {
      PRINTF_RNG_FAILED("dwr: reset: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, status);
    }
#endif
    status = 0;
    if(state == S_RESET || state == S_ABORT) {
      ranging_data.status = 0;
    } else if(state == S_RANGING_DONE || state == S_RANGING_DONE_MSG4) {
      double tof;
      double not_corrected;

      if(rng_type == DW1000_RNG_SS) {
        tof = ss_tof_calc();
      } else {
        tof = ds_tof_calc();
      }

      not_corrected = tof * SPEED_OF_LIGHT;
      ranging_data.raw_distance = not_corrected;
#if DW1000_COMPENSATE_BIAS
      ranging_data.distance = not_corrected - dwt_getrangebias(channel, not_corrected, DW1000_PRF);
#else
      ranging_data.distance = not_corrected;
#endif

      //PRINTF_RNG("dwr: %d done %f, after bias %f\n", my_seqn, not_corrected, ranging_data.distance);
      ranging_data.status = 1;
    }
    if(req_process != PROCESS_NONE) {
      if(ranging_data.status == 0 || state == S_RANGING_DONE_MSG4) {
        /* delay to let the 4th message be transmitted
         * or to let our peer timeout if we were interrupted
         * in the middle of a ranging sequence */
        etimer_set(&abort_timer, ranging_conf.finish_delay);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&abort_timer));
      }
      process_post(req_process, ranging_event, &ranging_data);
      req_process = PROCESS_NONE;
    }
    old_state = state;
    state = S_WAIT_POLL;
  }

  PROCESS_END();
}
#if DEBUG
PROCESS_THREAD(dw1000_rng_dbg_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();
  while(1) {
    etimer_set(&et, CLOCK_SECOND * 5);
    PROCESS_WAIT_EVENT();
    if(etimer_expired(&et)) {
      PRINTF_RNG_FAILED("dwr: periodic: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, status);
    }
  }
  PROCESS_END();
}
#endif /* DEBUG */
/*---------------------------------------------------------------------------*/
bool
dw1000_is_ranging()
{
  return state != S_WAIT_POLL;
}
/*---------------------------------------------------------------------------*/
/* Should be called with interrupts disabled */
void
dw1000_range_reset()
{
  switch(state) {
  case S_WAIT_SS1:
  case S_WAIT_DS1:
  case S_WAIT_DS2:
  case S_WAIT_DS3:
  /* case S_ABORT: */
  /* case S_RANGING_DONE: */
  /* case S_RANGING_DONE_MSG4: */
    old_state = state;
    state = S_RESET;
    process_poll(&dw1000_rng_process);
    break;
  default:
    break;
  }
}

void dw1000_ranging_enable_cir_print(bool enable){
  print_cir_requested = enable;
}
/*---------------------------------------------------------------------------*/
#endif /* DW1000_RANGING_ENABLED */
