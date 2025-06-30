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
#include "dw1000-shared-state.h"
#include "dw1000-util.h"
#include "dw1000-cir.h"
#include "process.h"
#include "deca_range_tables.h"
#include "frame802154.h"
/*---------------------------------------------------------------------------*/
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#if DW1000_RANGING_ENABLED

#if (DW1000_FRAMEFILTER == 0)
#warning HW frame filtering disabled!
#endif

// print module state periodically
#define DEBUG_RNG_PERIODIC 0

// print debug info on failed ranging attempts
#define DEBUG_RNG_FAILED 0

// print inside interrupts (typically dangerous)
#define DEBUG_RNG_INT 0

// print debug info on all ranging attempts
#ifndef DEBUG_RNG
#define DEBUG_RNG 0
#endif

#if DEBUG_RNG
#include <stdio.h>
#define PRINTF_RNG(...) printf(__VA_ARGS__)
#else
#define PRINTF_RNG(...) do {} while(0)
#endif

#if DEBUG_RNG_FAILED
#include <stdio.h>
#define PRINTF_RNG_FAILED(...) printf(__VA_ARGS__)
#else
#define PRINTF_RNG_FAILED(...) do {} while(0)
#endif

#if DEBUG_RNG_INT
#include <stdio.h>
#define PRINTF_INT(...) printf(__VA_ARGS__)
#else
#define PRINTF_INT(...) do {} while(0)
#endif

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

process_event_t ranging_event;
struct process *req_process;
static ranging_data_t ranging_data;
static int err_status;
static dw1000_rng_type_t rng_type;
static dw1000_cir_sample_t* cir_buffer;
static bool acquire_diagnostics;
static int16_t cir_s1;
static uint16_t cir_idx_mode;
static uint16_t cir_n_samples;

typedef enum {
  S_WAIT_POLL,        /* 0 */
  S_WAIT_SS1,         /* 1 */
  S_WAIT_SS1_DONE,    /* 2 */
  S_WAIT_DS1,         /* 3 */
  S_WAIT_DS2,         /* 4 */
  S_WAIT_DS3,         /* 5 */
  S_WAIT_DS3_DONE,    /* 6 */
  S_RANGING_DONE,     /* 7 */
  S_ABORT,            /* 8 */
  S_RESET             /* 9 */
} state_t;

static state_t state;
static state_t old_state;



static const frame802154_t default_header = {
  .fcf = {
    .frame_type        = 1, // data
    .security_enabled  = 0,
    .frame_pending     = 0,
    .ack_required      = 0,
    .panid_compression = 1, // suppress src PAN ID
    .sequence_number_suppression = 0,
    .ie_list_present   = 0,
    .dest_addr_mode    = (LINKADDR_SIZE == 8) ? 3 : 2,
    .frame_version     = 0,
    .src_addr_mode     = (LINKADDR_SIZE == 8) ? 3 : 2,
  },
  .dest_pid = IEEE802154_PANID,
  .src_pid  = IEEE802154_PANID,
};


/* Packet lengths for different messages (include the 2-byte CRC!) */
#define PLD_LEN_POLL 1

#define PLD_LEN_SS1 9

#define PLD_LEN_DS1 1
#define PLD_LEN_DS2 13
#define PLD_LEN_DS3 13

/* Packet types for different messages */
#define MSG_TYPE_SS0 0xE0
#define MSG_TYPE_SS1 0xE1

#define MSG_TYPE_DS0 0xD0
#define MSG_TYPE_DS1 0xD1
#define MSG_TYPE_DS2 0xD2
#define MSG_TYPE_DS3 0xD3

/* Indexes to access some of the fields in the frames defined above. */
#define PLD_TYPE_OFS 0

/*SS*/
#define RESP_MSG_POLL_RX_TS_OFS 1
#define RESP_MSG_RESP_TX_TS_OFS 5

/*DS*/
#define FINAL_MSG_POLL_TX_TS_OFS 1
#define FINAL_MSG_RESP_RX_TS_OFS 5
#define FINAL_MSG_FINAL_TX_TS_OFS 9

#define DISTANCE_MSG_POLL_RX_OFS 1
#define DISTANCE_MSG_RESP_TX_OFS 5
#define DISTANCE_MSG_FINAL_RX_OFS 9

static uint8_t my_seqn;
static uint8_t recv_seqn;
static linkaddr_t ranging_with; /* the current ranging peer */

#define MAX_BUF_LEN 36
static uint8_t rtx_buf[MAX_BUF_LEN];

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

} ranging_conf_t;

/*
* tx: tx_ts
*           ----------- DS0/SS0 ----------->
* listen:  txdone + rx_dly_a
* timeout: txdone + rx_dly_a + to_a         tx: rx_ts + a
*           <---------- DS1/SS1 ------------
*                                           listen:  txdone + rx_dly_b
* tx: rx_ts + b                             timeout: txdone + rx_dly_b + to_b
*           ------------- DS2 ------------->
* timeout: txdone + to_c                    tx: asap
*           <------------ DS3 --------------
*
*
* NB: Timing of the ranging exchange depends on the data rate, preamble length,
* and packet sizes. Consult tools/ranging_timing.py for calculations.
*/

#if DW1000_EXTREME_RNG_TIMING
/* The following are tuned for 128us preamble, EVB1000, long addresses */
const static ranging_conf_t ranging_conf_6M8 = {
/* SS and DS timeouts */
  .a = 350,
  .rx_dly_a = 100,    // timeout starts after this
  .to_a = 300,

/* DS timeouts */
  .b = 450,
  .rx_dly_b = 200,    // timeout starts after this
  .to_b = 300,
  .to_c = 400,        // longer as there's no rx after tx delay
};
#else
/* The following are tuned for 128us preamble, DWM1001 with BLE enabled, long addresses */
const static ranging_conf_t ranging_conf_6M8 = {
/* SS and DS timeouts */
  .a = 650,
  .rx_dly_a = 400,    // timeout starts after this
  .to_a = 300,

/* DS timeouts */
  .b = 650,
  .rx_dly_b = 400,    // timeout starts after this
  .to_b = 300,
  .to_c = 650,        // longer as there's no rx after tx delay
};
#endif

const static ranging_conf_t ranging_conf_850K = {
/* SS and DS timeouts */
  .a = 1500,
  .rx_dly_a = 0,
  .to_a = 2000,

/* DS timeouts */
  .b = 1500,
  .rx_dly_b = 0,
  .to_b = 3000,
  .to_c = 2500,
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
};

static ranging_conf_t ranging_conf;

/* Update ranging delays based on current radio config */
static inline void update_ranging_conf(void) {
  switch(dw1000_cached_config.cfg.dataRate) {
  case DWT_BR_6M8:
    ranging_conf = ranging_conf_6M8;
    break;
  case DWT_BR_850K:
    ranging_conf = ranging_conf_850K;
    break;
  case DWT_BR_110K:
    ranging_conf = ranging_conf_110K;
    break;
  }
}

/*---------------------------------------------------------------------------*/
static inline uint64_t
get_rx_timestamp_u64(void)
{
  uint64_t ts = 0;
  dwt_readrxtimestamp((uint8_t*)&ts);
  return ts;
}
/*---------------------------------------------------------------------------*/
static inline uint64_t
get_tx_timestamp_u64(void)
{
  uint64_t ts = 0;
  dwt_readtxtimestamp((uint8_t*)&ts);
  return ts;
}
/*---------------------------------------------------------------------------*/
static inline void
msg_get_u32(uint8_t *ts_field, uint32_t *ts)
{
  *ts = 0;
  for(int i = 3; i >= 0; i--) {
    *ts <<= 8;
    *ts |= ts_field[i];
  }
}
/*---------------------------------------------------------------------------*/
static inline void
msg_set_u32(uint8_t *ts_field, uint32_t ts)
{
  for(int i = 0; i < 4; i++) {
    ts_field[i] = (uint8_t)ts;
    ts >>= 8;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(dw1000_rng_process, "DW1000 rng process");
#if DEBUG_RNG_PERIODIC
PROCESS(dw1000_rng_dbg_process, "DW1000 rng dbg process");
#endif
/*---------------------------------------------------------------------------*/

#define PROFILE_RANGING 0
#if PROFILE_RANGING
  rtimer_clock_t r_start, r_done, r_proc, r_calc, r_cir;
#endif


/* Initialise the ranging module. */
void
dw1000_ranging_init(void)
{
  process_start(&dw1000_rng_process, NULL);
  state = S_WAIT_POLL;
  old_state = state;
}
/*---------------------------------------------------------------------------*/
bool
dw1000_range_with(linkaddr_t *lladdr, dw1000_rng_type_t type)
{
  int8_t irq_status;
  bool ret;
#if PROFILE_RANGING
  r_start = RTIMER_NOW();
#endif
  frame802154_t frame = default_header;
  if(type != DW1000_RNG_SS && type != DW1000_RNG_DS) {
    return false;
  }

  if(!ranging_event) {
    return false; /* first call the init function */
  }
  if(req_process != PROCESS_NONE) {
    PRINTF_RNG_FAILED("dwr: busy 1: ost %d st %d ss %d\n", old_state, state, err_status);
    return false; /* already ranging */
  }
  irq_status = dw1000_disable_interrupt();

  if(state != S_WAIT_POLL) {
    PRINTF_RNG_FAILED("dwr: busy 2: ost %d st %d ss %d\n", old_state, state, err_status);
    ret = false;
    goto enable_interrupts;
  }

  dwt_forcetrxoff();
  update_ranging_conf();

  ranging_with = *lladdr;
  ranging_data.status = 0;
  ranging_data.distance = 0;
  ranging_data.raw_distance = 0;
  rng_type = type;

  my_seqn++;

  PRINTF_RNG("dwr: rng start %d type %d\n", my_seqn, rng_type);

  /* fill in non-constant fields of the header */
  frame.seq = my_seqn;
  memcpy(frame.dest_addr, lladdr->u8, LINKADDR_SIZE);
  memcpy(frame.src_addr, linkaddr_node_addr.u8, LINKADDR_SIZE);

  /* create the header */
  uint8_t hdr_len = frame802154_create(&frame, rtx_buf);
  /* create the payload */
  rtx_buf[hdr_len + PLD_TYPE_OFS] = (rng_type == DW1000_RNG_SS) ? MSG_TYPE_SS0 : MSG_TYPE_DS0;

  /* Set expected response's delay and timeout.*/
  dwt_setrxaftertxdelay(ranging_conf.rx_dly_a);
  dwt_setrxtimeout(ranging_conf.to_a);

  /* Write frame data to DW1000 and prepare transmission. */
  dwt_writetxdata(hdr_len + PLD_LEN_POLL + DW1000_CRC_LEN, rtx_buf, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(hdr_len + PLD_LEN_POLL + DW1000_CRC_LEN, 0, 1); /* Zero offset in TX buffer, ranging. */

  /* Start transmission, indicating that a response is expected so that
   * reception is enabled automatically after the frame is sent and the delay
   * set by dwt_setrxaftertxdelay() has elapsed. */

  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  ret = true; /* the request is successful */
  old_state = state;
  state = (rng_type == DW1000_RNG_SS) ? S_WAIT_SS1 : S_WAIT_DS1;
  req_process = PROCESS_CURRENT();

enable_interrupts:
  dw1000_enable_interrupt(irq_status);
  PRINTF_RNG("dwr: exit %d\n", my_seqn);
  return ret;
}
/*---------------------------------------------------------------------------*/
/* Timestamps needed for SS computations */
static uint32_t ss_poll_tx_ts, ss_resp_rx_ts, ss_poll_rx_ts, ss_resp_tx_ts;
/* Timestamps needed for DS computations */
static uint32_t ds_poll_tx_ts, ds_resp_rx_ts, ds_final_tx_ts;
static uint32_t ds_poll_rx_ts, ds_resp_tx_ts, ds_final_rx_ts;

// clock frequency offset to compensate the distance bias in SS-TWR
static double clockOffsetPPM;


/*---------------------------------------------------------------------------*/
/* Callback to process ranging good frame events
 */
void
dw1000_rng_ok_cb(const dwt_cb_data_t *cb_data)
{
  uint16_t pkt_len = cb_data->datalength;
  uint8_t rx_hdr_len, tx_hdr_len;
  frame802154_t rx_frame;
  frame802154_t tx_frame = default_header;
  uint8_t rx_type;

  if (pkt_len - DW1000_CRC_LEN > MAX_BUF_LEN) {
    err_status = 20;
    goto abort;
  }

  dwt_readrxdata(rtx_buf, pkt_len - DW1000_CRC_LEN, 0);
  rx_hdr_len = frame802154_parse(rtx_buf, pkt_len - DW1000_CRC_LEN, &rx_frame);

  if(!rx_hdr_len) {
    err_status = 21;
    goto abort;
  }

  // TODO: if HW frame filtering is disabled, add address check here

  uint8_t  pld_len = rx_frame.payload_len;
  uint8_t* pld     = rx_frame.payload;

  rx_type = pld[PLD_TYPE_OFS];
  recv_seqn = rx_frame.seq;

  if(state == S_WAIT_POLL) {
    if(pld_len != PLD_LEN_POLL) {
      err_status = 11;
      goto abort;
    }

    update_ranging_conf();

    if(rx_type == MSG_TYPE_SS0) {  /* --- Single-sided poll --- */

      /* Timestamps of frames transmission/reception.
       * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
      uint64_t poll_rx_ts_64;
      uint64_t resp_tx_ts_64;
      uint32_t resp_tx_time;

      PRINTF_INT("dwr: got SS0.\n");
      if (! (cb_data->status & SYS_STATUS_LDEDONE)) {
        PRINTF_RNG_FAILED("SS0: LDE failed\n");
        err_status = 16;
        goto abort;
      }
      /* Retrieve poll reception timestamp. */
      poll_rx_ts_64 = get_rx_timestamp_u64();

      /* Compute final message transmission time. */
      resp_tx_time = (poll_rx_ts_64 + (ranging_conf.a * UUS_TO_DWT_TIME)) >> 8;

      /* Request sending the delayed response */
      dwt_setdelayedtrxtime(resp_tx_time); // TX delay
      dwt_setrxaftertxdelay(0);            // enable RX right after TX
      dwt_setrxtimeout(0);                 // RX without timeout
      dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
      int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
      if(ret != DWT_SUCCESS) {
        err_status = 14;
        PRINTF_RNG_FAILED("dwr: error in tx of SS1.\n");
        goto abort;
      }

      /* Temporarily disable auto FCS. In case we are late filling the packet, the receiver understands 
       * that the packet is corrupted (see DW1000 User Manual) */
      dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_SFCST);

      /* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
      resp_tx_ts_64 = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + 
                          dw1000_cached_config.tx_ant_dly;

      /* Fill in the response header */
      tx_frame.seq = rx_frame.seq;
      memcpy(tx_frame.src_addr, linkaddr_node_addr.u8, LINKADDR_SIZE);
      memcpy(tx_frame.dest_addr, rx_frame.src_addr, LINKADDR_SIZE);
      tx_hdr_len = frame802154_create(&tx_frame, rtx_buf);

      /* Set up the PHY header */
      dwt_writetxfctrl(tx_hdr_len + PLD_LEN_SS1 + DW1000_CRC_LEN, 0, 1); /* Zero offset in TX buffer, ranging. */

      /* Fill in the SS1 payload */
      rtx_buf[tx_hdr_len + PLD_TYPE_OFS] = MSG_TYPE_SS1;
      msg_set_u32(&rtx_buf[tx_hdr_len + RESP_MSG_POLL_RX_TS_OFS], (uint32_t)poll_rx_ts_64); // we send 4 least-significant bytes
      msg_set_u32(&rtx_buf[tx_hdr_len + RESP_MSG_RESP_TX_TS_OFS], (uint32_t)resp_tx_ts_64); // of the 40-bit precise timestamps

      /* Write the frame data */
      dwt_writetxdata(tx_hdr_len + PLD_LEN_SS1 + DW1000_CRC_LEN, rtx_buf, 0);

      /* Cancel disabling auto-FCS, if we are in time, the radio will include FCS */
      dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_CANSFCS);
      
      old_state = state;
      state = S_WAIT_SS1_DONE;
      return; // replied with SS1, now waiting for TX done event.

    } else if(rx_type == MSG_TYPE_DS0) { /* --- Double-sided poll --- */
      uint32_t resp_tx_time;
      uint64_t poll_rx_ts_64;

      PRINTF_INT("dwr: got DS0.\n");

      if (! (cb_data->status & SYS_STATUS_LDEDONE)) {
        PRINTF_RNG_FAILED("DS0: LDE failed\n");
        err_status = 17;
        goto abort;
      }

      /* Retrieve poll and store poll reception timestamp. */
      poll_rx_ts_64 = get_rx_timestamp_u64();
      ds_poll_rx_ts = (uint32_t)poll_rx_ts_64;

      /* Set send time for response. */
      resp_tx_time = (poll_rx_ts_64 + (ranging_conf.a * UUS_TO_DWT_TIME)) >> 8;
      dwt_setdelayedtrxtime(resp_tx_time);

      /* Set expected delay and timeout for message reception. */
      dwt_setrxaftertxdelay(ranging_conf.rx_dly_b);
      dwt_setrxtimeout(ranging_conf.to_b);

      /* Request sending the response */
      dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
      int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
      if(ret != DWT_SUCCESS) {
        err_status = 15;
        PRINTF_RNG_FAILED("dwr: error in tx of DS1.\n");
        goto abort;
      }

      /* Temporarily disable auto FCS. In case we are late filling the packet, the receiver understands 
       * that the packet is corrupted (see DW1000 User Manual) */
      dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_SFCST);

      memcpy(ranging_with.u8, rx_frame.src_addr, LINKADDR_SIZE);

      /* Fill in the response header */
      tx_frame.seq = rx_frame.seq;
      memcpy(tx_frame.src_addr, linkaddr_node_addr.u8, LINKADDR_SIZE);
      memcpy(tx_frame.dest_addr, rx_frame.src_addr, LINKADDR_SIZE);
      tx_hdr_len = frame802154_create(&tx_frame, rtx_buf);

      /* Set up the PHY header */
      dwt_writetxfctrl(tx_hdr_len + PLD_LEN_DS1 + DW1000_CRC_LEN, 0, 1); /* Zero offset in TX buffer, ranging. */

      /* Fill in the DS1 payload */
      rtx_buf[tx_hdr_len + PLD_TYPE_OFS] = MSG_TYPE_DS1;

      /* Write the frame data. */
      dwt_writetxdata(tx_hdr_len + PLD_LEN_DS1 + DW1000_CRC_LEN, rtx_buf, 0); /* Zero offset in TX buffer. */

      /* Cancel disabling auto-FCS, if we are in time, the radio will include FCS */
      dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_CANSFCS);

      old_state = state;
      state = S_WAIT_DS2;
      return; // Replied with DS1. Now waiting for incoming DS2.
    } else {
      err_status = 13;
      goto abort;
    }
  } else if(state == S_WAIT_SS1) { /* --- We are waiting for the SS1 response ---- */
    if(pld_len != PLD_LEN_SS1) {
      err_status = 1;
      goto abort;
    }

    /* Check that the frame is the expected response */
    if(rx_type != MSG_TYPE_SS1) {
      err_status = 3;
      goto abort;
    }

    /* Retrieve poll transmission and response reception timestamps. */
    ss_poll_tx_ts = dwt_readtxtimestamplo32();
    ss_resp_rx_ts = dwt_readrxtimestamplo32();

    /* Get timestamps embedded in response message. */
    msg_get_u32(&rtx_buf[rx_hdr_len + RESP_MSG_POLL_RX_TS_OFS], &ss_poll_rx_ts);
    msg_get_u32(&rtx_buf[rx_hdr_len + RESP_MSG_RESP_TX_TS_OFS], &ss_resp_tx_ts);

    old_state = state;
    state = S_RANGING_DONE;

    goto poll_the_process; // ranging done, poll the process.
  } else if(state == S_WAIT_DS1) { /* --- We are waiting for the DS1 response --- */
    if(pld_len != PLD_LEN_DS1) {
      err_status = 41;
      goto abort;
    }

    /* Check that the frame is the expected response */
    if(rx_type != MSG_TYPE_DS1) {
      err_status = 44;
      goto abort;
    }

    PRINTF_INT("dwr: got DS1.\n");

    if (! (cb_data->status & SYS_STATUS_LDEDONE)) {
      PRINTF_RNG_FAILED("DS1: LDE failed\n");
      err_status = 18;
      goto abort;
    }

    uint64_t final_tx_ts_64;
    uint32_t final_tx_time;
    uint64_t poll_tx_ts_64, resp_rx_ts_64;

    /* Retrieve poll transmission and response reception timestamp. */
    poll_tx_ts_64 = get_tx_timestamp_u64();
    resp_rx_ts_64 = get_rx_timestamp_u64();

    /* Compute final message transmission time. */
    final_tx_time = (resp_rx_ts_64 + (ranging_conf.b * UUS_TO_DWT_TIME)) >> 8;
    dwt_setdelayedtrxtime(final_tx_time);

    dwt_setrxaftertxdelay(0);             // Enable RX right after TX
    dwt_setrxtimeout(ranging_conf.to_c);  // Set the RX timeout
    dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M); /* errata TX-1: force rx timeout */
    /* Request transmission of the response */
    int ret = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);

    if(ret != DWT_SUCCESS) {
      err_status = 45;
      PRINTF_RNG_FAILED("dwr: error in tx of DS2.\n");
      goto abort;
    }

    /* Temporarily disable auto FCS. In case we are late filling the packet, the receiver understands 
     * that the packet is corrupted (see DW1000 User Manual) */
    dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_SFCST);

    /* Final TX timestamp is the transmission time we programmed plus the TX antenna delay. */
    final_tx_ts_64 = (((uint64_t)(final_tx_time & 0xFFFFFFFEUL)) << 8) +
                          dw1000_cached_config.tx_ant_dly;

    ds_poll_tx_ts  = (uint32_t)poll_tx_ts_64;    // we send 4 least-significant bytes   
    ds_resp_rx_ts  = (uint32_t)resp_rx_ts_64;    // of the 40-bit precise timestamps
    ds_final_tx_ts = (uint32_t)final_tx_ts_64;

    /* Fill in the response header */
    // TODO: can we reuse it from the previous time (DS0) ?
    tx_frame.seq = rx_frame.seq;
    memcpy(tx_frame.src_addr, linkaddr_node_addr.u8, LINKADDR_SIZE);
    memcpy(tx_frame.dest_addr, rx_frame.src_addr, LINKADDR_SIZE);
    tx_hdr_len = frame802154_create(&tx_frame, rtx_buf);

    /* Set up the PHY header */
    dwt_writetxfctrl(tx_hdr_len + PLD_LEN_DS2 + DW1000_CRC_LEN, 0, 1); /* Zero offset in TX buffer, ranging. */

    /* Fill in the DS2 payload */
    rtx_buf[tx_hdr_len + PLD_TYPE_OFS] = MSG_TYPE_DS2;

    msg_set_u32(&rtx_buf[tx_hdr_len + FINAL_MSG_POLL_TX_TS_OFS],  ds_poll_tx_ts);
    msg_set_u32(&rtx_buf[tx_hdr_len + FINAL_MSG_RESP_RX_TS_OFS],  ds_resp_rx_ts);
    msg_set_u32(&rtx_buf[tx_hdr_len + FINAL_MSG_FINAL_TX_TS_OFS], ds_final_tx_ts);

    /* Write the frame data */
    dwt_writetxdata(tx_hdr_len + PLD_LEN_DS2 + DW1000_CRC_LEN, rtx_buf, 0); /* Zero offset in TX buffer. */

    /* Cancel disabling auto-FCS, if we are in time, the radio will include FCS */
    dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_CANSFCS);

    old_state = state;
    state = S_WAIT_DS3;
    return; // Sent DS2, wait for incoming DS3 message.

  } else if(state == S_WAIT_DS2) { /* --- We are waiting for the DS2 response --- */
    if(pld_len != PLD_LEN_DS2) {
      err_status = 51;
      goto abort;
    }

    /* Check that the frame is the expected response */
    if(rx_type != MSG_TYPE_DS2) {
      err_status = 54;
      goto abort;
    }

    PRINTF_INT("dwr: got DS2.\n");
    if (! (cb_data->status & SYS_STATUS_LDEDONE)) {
      PRINTF_RNG_FAILED("DS2: LDE failed\n");
      err_status = 19;
      goto abort;
    }

    /* ds_poll_rx_ts was stored on the previous step */
    /* Retrieve response transmission and final reception timestamps. */
    ds_resp_tx_ts =  (uint32_t)get_tx_timestamp_u64();
    ds_final_rx_ts = (uint32_t)get_rx_timestamp_u64();

    /* Get timestamps embedded in the final message. */
    msg_get_u32(&rtx_buf[rx_hdr_len + FINAL_MSG_POLL_TX_TS_OFS],  &ds_poll_tx_ts);
    msg_get_u32(&rtx_buf[rx_hdr_len + FINAL_MSG_RESP_RX_TS_OFS],  &ds_resp_rx_ts);
    msg_get_u32(&rtx_buf[rx_hdr_len + FINAL_MSG_FINAL_TX_TS_OFS], &ds_final_tx_ts);

    /* Fill in the response header */
    // TODO: can we reuse it from the previous time (DS0) ?
    tx_frame.seq = rx_frame.seq;
    memcpy(tx_frame.src_addr, linkaddr_node_addr.u8, LINKADDR_SIZE);
    memcpy(tx_frame.dest_addr, rx_frame.src_addr, LINKADDR_SIZE);
    tx_hdr_len = frame802154_create(&tx_frame, rtx_buf);

    /* Fill in the DS3 payload */
    rtx_buf[tx_hdr_len + PLD_TYPE_OFS] = MSG_TYPE_DS3;

    memcpy(&rtx_buf[tx_hdr_len + DISTANCE_MSG_POLL_RX_OFS],  &ds_poll_rx_ts,  4);
    memcpy(&rtx_buf[tx_hdr_len + DISTANCE_MSG_RESP_TX_OFS],  &ds_resp_tx_ts,  4);
    memcpy(&rtx_buf[tx_hdr_len + DISTANCE_MSG_FINAL_RX_OFS], &ds_final_rx_ts, 4);

    dwt_writetxfctrl(tx_hdr_len + PLD_LEN_DS3 + DW1000_CRC_LEN, 0, 1); /* Zero offset in TX buffer, ranging. */
    dwt_writetxdata(tx_hdr_len + PLD_LEN_DS3 + DW1000_CRC_LEN, rtx_buf, 0); /* Zero offset in TX buffer. */
    dwt_starttx(DWT_START_TX_IMMEDIATE); // not enabling reception, the process will do it later

    PRINTF_INT("dwr: sent DS3.\n");

    /* done with this ranging session but still sending the 4th message */
    old_state = state;
    state = S_WAIT_DS3_DONE;
    return; // sent DS3. Wait for TX done event.

  } else if(state == S_WAIT_DS3) { /* --- We are waiting for the DS3 response --- */
    if(pld_len != PLD_LEN_DS3) {
      err_status = 61;
      goto abort;
    }

    /* Check that the frame is the expected response */
    if(rx_type != MSG_TYPE_DS3) {
      err_status = 64;
      goto abort;
    }

    PRINTF_INT("dwr: got DS3.\n");

    msg_get_u32(&rtx_buf[rx_hdr_len + DISTANCE_MSG_POLL_RX_OFS],  &ds_poll_rx_ts);
    msg_get_u32(&rtx_buf[rx_hdr_len + DISTANCE_MSG_RESP_TX_OFS],  &ds_resp_tx_ts);
    msg_get_u32(&rtx_buf[rx_hdr_len + DISTANCE_MSG_FINAL_RX_OFS], &ds_final_rx_ts);

    old_state = state;
    state = S_RANGING_DONE;
    goto poll_the_process; // ranging done, poll the process
  }

abort:
  /* In case we got anything unexpected or could not TX. Radio is OFF. */
  old_state = state;
  state = S_ABORT;
poll_the_process:
#if PROFILE_RANGING
  r_done = RTIMER_NOW();
#endif
  /* Ranging is done, either successfully or not. Radio is OFF.
   * Polling the process to notify the application and re-enable the reception. */
  process_poll(&dw1000_rng_process);
}
/*---------------------------------------------------------------------------*/
/* Callback to process tx confirmation events
 */
void
dw1000_rng_tx_conf_cb(const dwt_cb_data_t *cb_data) {
  PRINTF_INT("done %d %d\n", old_state, state);
  if (state == S_WAIT_POLL) // not ranging
    return;

  dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, 0); /* disable the errata TX-1 workaround (to save energy) */

  if (state == S_WAIT_SS1_DONE) {
    old_state = state;
    state = S_WAIT_POLL;
    // We responded to the ranging request, no need to notify the process.
    // The radio is ON as requested in dwt_starttx for SS1.
  }
  else if (state == S_WAIT_DS3_DONE) {
    old_state = state;
    state = S_RANGING_DONE;
    // The ranging exchange is done.
    // Invoking the process to compute the distance and re-enable the radio.
    process_poll(&dw1000_rng_process);
  }
}

static double retrieve_clock_offset(void)
{
  /* Read and store carrier integrator value */
  int32 carrierIntegrator = dwt_readcarrierintegrator();
  
  double hertz_to_ppm_multiplier = dw1000_get_hz2ppm_multiplier(
    &dw1000_cached_config.cfg);

  return carrierIntegrator * hertz_to_ppm_multiplier;
}

static double
ss_tof_calc(void)
{
  int32_t rtd_init, rtd_resp;
  /* Compute time of flight. */
  rtd_init = ss_resp_rx_ts - ss_poll_tx_ts;
  rtd_resp = ss_resp_tx_ts - ss_poll_rx_ts;

  double clockOffsetRatio = clockOffsetPPM / 1.0e6;

  return ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS; // with clock drift compensation
  //return ((rtd_init - rtd_resp) / 2.0) * DWT_TIME_UNITS; // without the compensation
}
/*---------------------------------------------------------------------------*/
static double
ds_tof_calc(void)
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
  PROCESS_BEGIN();

  if(!ranging_event) {
    ranging_event = process_alloc_event();
  }

#if DEBUG_RNG_PERIODIC
  process_start(&dw1000_rng_dbg_process, NULL);
#endif

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

#if PROFILE_RANGING
    r_proc = RTIMER_NOW();
#endif
    uint8_t irq_status = dw1000_disable_interrupt();

    PRINTF_RNG("dwr: process: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, err_status);
#if DEBUG_RNG == 0
    if(state == S_ABORT || state == S_RESET) {
      PRINTF_RNG_FAILED("dwr: abort/reset: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, err_status);
    }
#endif
    err_status = 0;

    if(state == S_RANGING_DONE) {
      double tof;
      double not_corrected;

      /* Clock offset is strictly necessary for SS TWR, but we acquiring it also 
       * in case of DS just for the sake of completeness */
      clockOffsetPPM = retrieve_clock_offset();

      if(rng_type == DW1000_RNG_SS) {
        tof = ss_tof_calc();
        ranging_data.poll_tx_ts = ss_poll_tx_ts;
        ranging_data.resp_rx_ts = ss_resp_rx_ts;
        ranging_data.poll_rx_ts = ss_poll_rx_ts;
        ranging_data.resp_tx_ts = ss_resp_tx_ts;
        ranging_data.ds_final_tx_ts = 0;
        ranging_data.ds_final_rx_ts = 0;
      } else {
        tof = ds_tof_calc();
        ranging_data.poll_tx_ts = ds_poll_tx_ts;
        ranging_data.resp_rx_ts = ds_resp_rx_ts;
        ranging_data.poll_rx_ts = ds_poll_rx_ts;
        ranging_data.resp_tx_ts = ds_resp_tx_ts;
        ranging_data.ds_final_tx_ts = ds_final_tx_ts;
        ranging_data.ds_final_rx_ts = ds_final_rx_ts;
      }

      not_corrected = tof * SPEED_OF_LIGHT;
      ranging_data.raw_distance = not_corrected;
#if DW1000_COMPENSATE_BIAS
      ranging_data.distance = not_corrected - dwt_getrangebias(
          dw1000_cached_config.cfg.chan, 
          not_corrected, 
          dw1000_cached_config.cfg.prf);
#else
      ranging_data.distance = not_corrected;
#endif
      ranging_data.clock_offset_ppm = clockOffsetPPM;

      //PRINTF_RNG("dwr: %d done %f, after bias %f\n", my_seqn, not_corrected, ranging_data.distance);
      ranging_data.status = 1;
    }
    else {
      ranging_data.status = 0; // no distance
    }

#if PROFILE_RANGING
    r_calc = RTIMER_NOW();
#endif

    ranging_data.cir_samples_acquired = 0;
    if (state == S_RANGING_DONE && acquire_diagnostics) {
      dwt_readdiagnostics(&ranging_data.rxdiag);

      if (cir_idx_mode == DW1000_CIR_IDX_RELATIVE) {
        // take the integer part of the FP index and add the relative shift
        cir_s1 = (ranging_data.rxdiag.firstPath >> 6) + cir_s1; 
      } 
      
      if (cir_s1 >= 0 && cir_buffer) {
        ranging_data.cir_samples_acquired = dw1000_read_cir(cir_s1, cir_n_samples, cir_buffer);
      }
      cir_buffer = NULL;
      acquire_diagnostics = false;
    }

#if PROFILE_RANGING
    r_cir = RTIMER_NOW();
#endif

    if (state != S_RESET) {
      // if no reset was requested, re-enable reception
      dwt_setrxtimeout(0);
      dwt_rxenable(DWT_START_RX_IMMEDIATE);
    }

    struct process *process_to_poll = req_process;
    req_process = PROCESS_NONE;
    old_state = state;
    state = S_WAIT_POLL;

    dw1000_enable_interrupt(irq_status);

#if PROFILE_RANGING
    printf("rng t %lu %lu %lu %lu %lu\n", r_start, r_done, r_proc, r_calc, r_cir);
    printf("rng d %ld %ld %ld %ld\n", r_done - r_start, r_proc - r_done, r_calc - r_proc, r_cir - r_calc);
#endif

    if(process_to_poll != PROCESS_NONE) {
      // calling back the requesting process right now (synch)
      // otherwise another process might corrupt the ranging_data
      // if activated before the requesting process
      process_post_synch(process_to_poll, ranging_event, &ranging_data);
    }

  }

  PROCESS_END();
}
#if DEBUG_RNG_PERIODIC
PROCESS_THREAD(dw1000_rng_dbg_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();
  while(1) {
    etimer_set(&et, CLOCK_SECOND * 10);
    PROCESS_WAIT_EVENT();
    if(etimer_expired(&et)) {
      printf("dwr: periodic: my %d their %d, ost %d st %d ss %d\n", my_seqn, recv_seqn, old_state, state, err_status);
    }
  }
  PROCESS_END();
}
#endif
/*---------------------------------------------------------------------------*/
bool
dw1000_is_ranging(void)
{
  return state != S_WAIT_POLL;
}
/*---------------------------------------------------------------------------*/
/* Should be called with interrupts disabled */
void
dw1000_range_reset(void)
{
  if (state != S_WAIT_POLL) {
    dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, 0); /* disable the errata TX-1 workaround (to save energy) */
    old_state = state;
    state = S_RESET;
    // invoke the process to notify the application (if needed)
    process_poll(&dw1000_rng_process);
  }
}

void dw1000_ranging_acquire_diagnostics(uint16_t idx_mode, int16_t s1, uint16_t n_samples, dw1000_cir_sample_t* samples) {
  acquire_diagnostics = true;
  cir_buffer = samples;
  cir_n_samples = n_samples;
  cir_s1 = s1;
  cir_idx_mode = idx_mode;
}
/*---------------------------------------------------------------------------*/
#endif /* DW1000_RANGING_ENABLED */
