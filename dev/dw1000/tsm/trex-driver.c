/*
 * Copyright (c) 2020, University of Trento.
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
 * \file      Low-level slot operation driver for the Time Slot Manager (TSM)
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 */

/* configure the debug output */
#define LOG_PREFIX "td"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "trex.h"
#include "trex-driver.h"
#include "contiki.h"
#include "dw1000.h"
#include "dw1000-config.h"
#include "dw1000-conv.h"
#include "dw1000-arch.h"
#include "dw1000-util.h"
#include "dw1000-statetime.h"
#include <string.h>
#include <stdio.h>

#if STATETIME_CONF_ON
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

enum trexd_state {
  TREXD_ST_IDLE,
  TREXD_ST_RX,
  TREXD_ST_TX,
  TREXD_ST_TIMER
};

static struct {
  enum trexd_state state;
  trexd_slot_cb cb;
  trexd_slot_t slot;
  uint32_t tx_antenna_delay_4ns;    // cache the antenna delay value
  uint32_t preamble_duration_4ns;   // cache the preamble duration
  uint16_t rx_slot_preambleto_pacs; // preamble detection timeout in PACs
} context;

/* Convert time from the ~4ns device time unit to UWB microseconds */
#define DTU_4NS_TO_UUS(NS4_TIME)    ((NS4_TIME) >> 8)

/* Convert time in the 15.65ps device time units to ~4ns units */
#define DTU_15PS_TO_4NS(PS15_TIME)  ((PS15_TIME) >> 8)

#define TREXD_FRAME_OVERHEAD (2)     // 2-byte CRC field


/* Compare two 32-bit timestamps.
 *
 * Pre-condition: the actual difference between the two timestamps is less than
 * half of the timer overflow range. For DW1000, the timer overflows once in
 * ~17 seconds, therefore t1 and t2 timestamps must be taken within ~8 seconds.
 *
 * Returns 
 *   - true if t2 is strictly later than t1, 
 *   - false if t2 is before than t1 or equal.
 */
static inline bool time32_lt(uint32_t t1, uint32_t t2) {
  return (int32_t)(t2-t1) > 0;
}

/*----------------------------------------------------------------------------*/
static trexd_stats_t stats;

static inline void update_rxok_stats() {stats.n_rxok++;}
static inline void update_txok_stats() {stats.n_txok++;}

static inline void update_rxto_stats(uint32_t status) {
  if (status & SYS_STATUS_RXPTO)          {
    stats.n_pto++;
  } else if (status & SYS_STATUS_RXRFTO)  {
    stats.n_fto++;
  } else {
    stats.n_unknown++;
  }
}

static inline void update_rxerr_stats(uint32_t status) {
  if (status & SYS_STATUS_RXPHE)          {
    stats.n_phe++;
  } else if (status & SYS_STATUS_RXSFDTO) {
    stats.n_sfdto++;
  } else if (status & SYS_STATUS_RXRFSL)  {
    stats.n_rse++;
  } else if (status & SYS_STATUS_RXFCE)   {
    stats.n_fcse++;
  } else if (status & SYS_STATUS_AFFREJ)  {
    stats.n_rej++;
  } else {
    stats.n_unknown++;
  }
}

/*----------------------------------------------------------------------------*/

static inline void slot_event() {
  DBGF();
  if (context.cb != NULL) {
    context.cb(&context.slot);
  }
}

static void
tx_done_cb(const dwt_cb_data_t *cbdata)
{
  STATETIME_MONITOR(dw1000_statetime_after_tx(dwt_readtxtimestamphi32(), context.slot.payload_len + TREXD_FRAME_OVERHEAD));
  context.slot.status = TREX_TX_DONE;
  context.slot.radio_status = cbdata->status;
  context.state = TREXD_ST_IDLE;
  update_txok_stats();
  slot_event();
}

static void
rx_ok_cb(const dwt_cb_data_t *cbdata)
{
  WARNIF(context.slot.buffer == NULL);
  context.slot.status = TREX_RX_SUCCESS;
  context.slot.trx_sfd_time_4ns = dwt_readrxtimestamphi32();
  STATETIME_MONITOR(dw1000_statetime_after_rx(context.slot.trx_sfd_time_4ns, cbdata->datalength));
  // TODO: check against the min and max configured PSDU lenght?
  dwt_readrxdata(context.slot.buffer, 
                 cbdata->datalength - TREXD_FRAME_OVERHEAD, 0);
  context.slot.payload_len = cbdata->datalength - TREXD_FRAME_OVERHEAD;
  context.slot.radio_status = cbdata->status;
  context.state = TREXD_ST_IDLE;
  update_rxok_stats();
  slot_event();
}

static void
rx_to_cb(const dwt_cb_data_t *cbdata)
{
  STATETIME_MONITOR(dw1000_statetime_after_rxerr(dwt_readsystimestamphi32()));

  if (context.state == TREXD_ST_TIMER) { // we were in the "timer mode"
    context.slot.status = TREX_TIMER_EVENT;
  }
  else { // we were in the reception mode
    update_rxto_stats(cbdata->status);
    context.slot.status = TREX_RX_TIMEOUT;
  }

  context.slot.radio_status = cbdata->status;
  context.state = TREXD_ST_IDLE;
  slot_event();
}

static void
rx_err_cb(const dwt_cb_data_t *cbdata)
{
  STATETIME_MONITOR(dw1000_statetime_after_rxerr(dwt_readsystimestamphi32()));
  update_rxerr_stats(cbdata->status);
  context.slot.radio_status = cbdata->status;
  context.slot.status = TREX_RX_ERROR;
  context.state = TREXD_ST_IDLE;
  slot_event();
}

/*----------------------------------------------------------------------------*/


int trexd_tx_at(uint8_t *buffer, uint8_t payload_len, uint32_t sfd_time_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_TX;

  DBG("TX at %lu now %lu", sfd_time_4ns, dwt_readsystimestamphi32());
  // schedule TX at the specified time
  // TODO: check that the TX duration does not exceed the deadline
  uint16_t psdu_len = payload_len + TREXD_FRAME_OVERHEAD;
  context.slot.trx_sfd_time_4ns = sfd_time_4ns;
  context.slot.payload_len = payload_len;
  context.slot.buffer = buffer;

  uint32_t ts_tx_4ns = sfd_time_4ns - context.tx_antenna_delay_4ns;
  dwt_writetxdata(psdu_len, buffer, 0);
  dwt_writetxfctrl(psdu_len, 0, 0);
  dwt_setdelayedtrxtime(ts_tx_4ns);
  /* errata TX-1: ensure TX done is issued */
  dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
  int res = dwt_starttx(DWT_START_TX_DELAYED);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_tx(sfd_time_4ns));
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_rx_slot(uint8_t *buffer, uint32_t expected_sfd_time_4ns, uint32_t deadline_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_RX;

  DBG("RX at %lu till %lu now %lu", expected_sfd_time_4ns, deadline_4ns, dwt_readsystimestamphi32());
  // sanity check that the deadline is after the sfd time
  WARNIF(!time32_lt(expected_sfd_time_4ns, deadline_4ns));

  context.slot.buffer = buffer;
  // schedule delayed reception with timeout
  uint32_t ts_rx_4ns = expected_sfd_time_4ns - context.preamble_duration_4ns;
  dwt_setdelayedtrxtime(ts_rx_4ns);
  dwt_setrxtimeout(DTU_4NS_TO_UUS(deadline_4ns - ts_rx_4ns));
  dwt_setpreambledetecttimeout(context.rx_slot_preambleto_pacs);
  int res = dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_rx(ts_rx_4ns));
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_rx_until(uint8_t *buffer, uint32_t deadline_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_RX;

  context.slot.buffer = buffer;
  uint32_t now = dwt_readsystimestamphi32();

  // sanity check that the deadline is in the future
  WARNIF(!time32_lt(now, deadline_4ns));

  // enable reception with timeout
  dwt_setrxtimeout(DTU_4NS_TO_UUS(deadline_4ns - now));
  dwt_setpreambledetecttimeout(0);
  int res = dwt_rxenable(DWT_START_RX_IMMEDIATE);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_rx(now));
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_rx(uint8_t *buffer) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_RX;

  context.slot.buffer = buffer;
  uint32_t now = dwt_readsystimestamphi32();
  // enable reception
  dwt_setrxtimeout(0);
  dwt_setpreambledetecttimeout(0);
  int res = dwt_rxenable(DWT_START_RX_IMMEDIATE);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_rx(now));
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_rx_from(uint8_t *buffer, uint32_t rx_on_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_RX;

  context.slot.buffer = buffer;
  // enable reception
  dwt_setdelayedtrxtime(rx_on_4ns);
  dwt_setrxtimeout(0);
  dwt_setpreambledetecttimeout(0);
  int res = dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_rx(rx_on_4ns));
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_set_timer(uint32_t deadline_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_TIMER;

  context.slot.buffer = NULL;
  context.slot.payload_len = 0;
  uint32_t now = dwt_readsystimestamphi32();

  // sanity check that the deadline is in the future
  WARNIF(!time32_lt(now, deadline_4ns));

  // enable a "fake" reception for 1 uus
  dwt_setrxtimeout(1);
  dwt_setdelayedtrxtime(deadline_4ns - 1 * UUS_TO_DWT_TIME_32);
  dwt_setpreambledetecttimeout(0);
  int res = dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;

  WARNIF(res!=DWT_SUCCESS);
  return res;
}


/*----------------------------------------------------------------------------*/

void trexd_init()
{
  uint16_t rx_ant_dly, tx_ant_dly;
  // Make sure the radio is off
  dwt_forcetrxoff();
  context.state = TREXD_ST_IDLE;

  // Set interrupt handlers
  dw1000_set_isr(dwt_isr);
  dwt_setcallbacks(&tx_done_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb);
  /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
  dwt_setinterrupt(
      DWT_INT_TFRS  | DWT_INT_RFCG | DWT_INT_RFTO |
      DWT_INT_RXPTO | DWT_INT_RPHE | DWT_INT_RFCE |
      DWT_INT_RFSL  | DWT_INT_SFDT | DWT_INT_ARFE , 1);

  /* Make sure frame filtering is disabled */
  dwt_enableframefilter(DWT_FF_NOTYPE_EN);

  /* Convert the current antenna delay to 4ns for future use */
  dw1000_get_current_ant_dly(&rx_ant_dly, &tx_ant_dly);
  context.tx_antenna_delay_4ns = DTU_15PS_TO_4NS(tx_ant_dly);
  context.preamble_duration_4ns = dw1000_estimate_tx_time(dw1000_get_current_cfg(), 0, true) / 4.0064102564;

  if (dw1000_get_current_cfg()->txPreambLength == DWT_PLEN_64) {
    // Use radio settings optimised for preamble length 64
    // XXX (it is unclear how to revert this once set)
    dwt_configurefor64plen(DWT_PRF_64M);

    // further optimisations recommended for preamble length 64
    /* // TODO: check whether this improves anything
       spix_change_speed(DW1000_SPI, DW1000_SPI_SLOW);
       dwt_loadopsettabfromotp(DWT_OPSET_64LEN);
       spix_change_speed(DW1000_SPI, DW1000_SPI_FAST);
       */
  }
  context.rx_slot_preambleto_pacs = 0; // set no preamble timeout initially

  STATETIME_MONITOR(dw1000_statetime_context_init());
}

void trexd_set_rx_slot_preambleto_pacs(const uint16_t preambleto_pacs)
{
    // NOTE: if set to 0 disables preamble timeout
    context.rx_slot_preambleto_pacs = preambleto_pacs;
}

// TODO: add limitations for acceptable frame dimensions?
// e.g., trexd_bound_frame_len(min, max)

void trexd_set_slot_callback(trexd_slot_cb callback)
{
  context.cb = callback;
}

void trexd_stats_reset()
{
  bzero(&stats, sizeof(stats));
}

void trexd_stats_print()
{
  // TODO: add a context key to group together multiple log lines
  
  PRINT("TREXD_STATS "
          "rxok %hu, txok %hu, "
          "pto %hu, fto %hu, "
          "phe %hu, sfdto %hu, "
          "rse %hu, fcse %hu, rej %hu",
          stats.n_rxok, stats.n_txok,
          stats.n_pto, stats.n_fto,
          stats.n_phe, stats.n_sfdto,
          stats.n_rse, stats.n_fcse, stats.n_rej);
}

void trexd_stats_get(trexd_stats_t* local_stats)
{
    *local_stats = stats;
}

