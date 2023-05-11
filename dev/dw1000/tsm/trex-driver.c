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
#include PROJECT_CONF_H

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
#include "evb1000-timer-mapping.h"

#include "clock.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "deca_driver_state.h"
extern dwt_local_data_t *pdw1000local;
static inline void custom_dwt_isr(void);

extern bool statetime_tracking;

static uint64_t complete_status;

#include "lib/random.h"  // Contiki random

#include "print-def.h"

#ifndef FS_DEBUG
#define FS_DEBUG 0
#endif

#pragma message STRDEF(FS_DEBUG)

#if FS_DEBUG
static uint32_t rx_start;
#endif

#if STATETIME_CONF_ON
#define STATETIME_MONITOR(...) __VA_ARGS__

// Expected deadlines
static uint32_t rxrfto_deadline = 0;
static uint32_t rxpto_deadline = 0;

// Signals if the last operation scheduled is to be considered for estimating the ratio between the mcu timer and the radio counter
static bool valid_deadline = false;
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

enum trexd_state {
  TREXD_ST_IDLE,
  TREXD_ST_RX,
  TREXD_ST_TX,
  TREXD_ST_TIMER,

#if TARGET == evb1000
  TREXD_ST_FP_ENABLED, // Needed for 3 phase FP (the other is TREXD_ST_IDLE for the disabled phase)
  TREXD_ST_FP_SENT,
  TREXD_ST_FP_SENT_DETECTED,
  TREXD_ST_FS_ORIGINATOR,
#endif
};

static struct {
  enum trexd_state state;
  trexd_slot_cb cb;
  trexd_slot_t slot;
  uint32_t tx_antenna_delay_4ns;    // cache the antenna delay value
  uint32_t preamble_duration_4ns;   // cache the preamble duration
  uint16_t rx_slot_preambleto_pacs; // preamble detection timeout in PACs
  uint32_t rx_slot_preambleto_duration_4ns; // corresponding durtion in 4ns of rx_slot_preambleto_pacs
} context;

/* Convert time from the ~4ns device time unit to UWB microseconds */
#define DTU_4NS_TO_UUS(NS4_TIME)    ((NS4_TIME) >> 8)

/* Convert time in the 15.65ps device time units to ~4ns units */
#define DTU_15PS_TO_4NS(PS15_TIME)  ((PS15_TIME) >> 8)

#define TREXD_FRAME_OVERHEAD (2)     // 2-byte CRC field

extern bool dw1000_is_sleeping;

/**
 * @brief Configures the SFD Timeout paramter of the radio 
 *
 * @param sfdTO Number of simbols to use as a timeout (number of symbols after the detection of the preamble after which the radio gives up detecting the SFD). Defaults to DWT_SFDTOC_DEF if 0
 * @return true if successful, false otherwise
 */
static inline bool dwt_configure_sfdTo(uint16_t sfdTO) { // NOTE: As we expect this to be called only by Flick it could be optimized further
    if (dw1000_is_sleeping) {
      PRINT("Err: Radio configure requested while sleeping");
      return false;
    }

    int8_t irq_status = dw1000_disable_interrupt();

    if(sfdTO == 0) {
        sfdTO = DWT_SFDTOC_DEF;
    }

    dwt_write16bitoffsetreg(DRX_CONF_ID, DRX_SFDTOC_OFFSET, sfdTO);

#ifndef SNIFF_FS
#define SNIFF_FS 0
#endif

#pragma message STRDEF(SNIFF_FS)
#pragma message STRDEF(SNIFF_FS_OFF_TIME)

#if SNIFF_FS
    if (sfdTO == 1) {
      dwt_setsniffmode(1, 1, SNIFF_FS_OFF_TIME);
    } else {
      dwt_setsniffmode(0, 1, SNIFF_FS_OFF_TIME);
    }

#endif

    dw1000_enable_interrupt(irq_status);

    return true;
}

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
    if (context.state == TREXD_ST_IDLE)
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
#if STATETIME_CONF_ON
  /*
   * If there we reached the end of a transmission and the process was 
   * enabled when scheudling the transmission, use the rmarker and the mcu timestamp 
   * of the transmission end to update the estimation of the ratio between the radio 
   * clock and the mcu timer clock
   */
  if (valid_deadline) {
    tm_update_tx(dwt_readtxtimestamphi32(), tm_get_timestamp(), context.slot.payload_len + TREXD_FRAME_OVERHEAD);
    valid_deadline = false;
  }
#endif

  if (context.state == TREXD_ST_FP_SENT || context.state == TREXD_ST_FP_SENT_DETECTED) {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits

    pdw1000local->wait4resp = 0;

    // Because of an issue with receiver restart after error conditions, an RX reset must be applied after any error or timeout event to ensure
    // the next good frame's timestamp is computed correctly.
    // See section "RX Message timestamp" in DW1000 User Manual.
    dwt_forcetrxoff();
    dwt_rxreset();

    if (context.state == TREXD_ST_FP_SENT) {
      STATETIME_MONITOR(dw1000_statetime_after_fs_pos(dwt_readtxtimestamphi32(), 3, true));
    } else if (context.state == TREXD_ST_FP_SENT_DETECTED){
      STATETIME_MONITOR(dw1000_statetime_after_fs_pos(dwt_readtxtimestamphi32(), 3, false));
    }

    // NOTE: This can probably made faster but it does not seem a problem for the processing times done until now (350us)
    dwt_configure_sfdTo(DW1000_CONF_SFD_TIMEOUT);
  } else {
    STATETIME_MONITOR(dw1000_statetime_after_tx(dwt_readtxtimestamphi32(), context.slot.payload_len + TREXD_FRAME_OVERHEAD));
  }

  if (context.state == TREXD_ST_FP_SENT) {
    context.slot.status = TREX_FS_DETECTED_AND_PROPAGATED;
  } else if (context.state == TREXD_ST_FP_SENT_DETECTED) {
    context.slot.status = TREX_FS_DETECTED;
  } else if (context.state == TREXD_ST_FS_ORIGINATOR) {
    context.slot.status = TREX_FS_DETECTED_AND_PROPAGATED;
  } else {
    context.slot.status = TREX_TX_DONE;
  }

  context.slot.radio_status = cbdata->status;
  context.state = TREXD_ST_IDLE;
  update_txok_stats();
  slot_event();
}

static void
rx_ok_cb(const dwt_cb_data_t *cbdata)
{
  WARNIF(cbdata->datalength>127);
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
  if (context.state == TREXD_ST_FP_ENABLED) {
    STATETIME_MONITOR(dw1000_statetime_after_fs_neg(dwt_readsystimestamphi32()));
  } else {
    STATETIME_MONITOR(dw1000_statetime_after_rxerr(dwt_readsystimestamphi32()));
  }

  if (context.state == TREXD_ST_TIMER) { // we were in the "timer mode"
    context.slot.status = TREX_TIMER_EVENT;
  } else if (context.state == TREXD_ST_FP_ENABLED){
    // We are in Flick reception mode

    // NOTE: As rx_preamble and rx_hunting have the same power consumption for the parameters used in log_parser2.py we can consider a negative an rxerr without any change to the final results (thus negative FS case can be handled without any change)
    context.state = TREXD_ST_IDLE;
    dwt_configure_sfdTo(DW1000_CONF_SFD_TIMEOUT);

    update_rxto_stats(cbdata->status);
    context.slot.status = TREX_FS_EMPTY;
  } else {
    // we are in normal reception mode
    update_rxto_stats(cbdata->status);
    context.slot.status = TREX_RX_TIMEOUT;
  }

#if STATETIME_CONF_ON

  /*
   * If the process was enabled when scheduling the reception , use the 
   * mcu timestamp of the reception (due RXRFTO or RXPTO) to update the 
   * estimation of the ratio between the radio clock and the mcu timer clock.
   * Note that in the case of RXPTO is important to filter for the RXPREJ
   * as we would otherwise consider cases in which a preamble was detected but
   * then rejected (which could behave differently from what we expect and thus
   * happen at a different instant than the precalculated rxpto_deadline
   */
  if (valid_deadline) {
    if (cbdata->status & SYS_STATUS_RXRFTO) { // If there we reached a valid deadline and it was set as a valid deadline
      tm_update_rxrfto(rxrfto_deadline, tm_get_timestamp());
    } else if ((cbdata->status & SYS_STATUS_RXPTO) && !(complete_status & SYS_STATUS_RXPREJ)) {
      tm_update_rxpto(rxpto_deadline, tm_get_timestamp());
    }

    valid_deadline = false;
  }
#endif

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
inline void trexd_tx_fp() {
  WARNIF(context.state != TREXD_ST_FP_SENT && context.state != TREXD_ST_FP_SENT_DETECTED);

  dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, (uint8)SYS_CTRL_TXSTRT);
}

int trexd_tx_at_fp(uint32_t sfd_time_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_FS_ORIGINATOR;

  DBG("TX at %lu now %lu", sfd_time_4ns, dwt_readsystimestamphi32());
  // schedule TX at the specified time
  // TODO: check that the TX duration does not exceed the deadline
  context.slot.trx_sfd_time_4ns = sfd_time_4ns;

  // NOTE: Maybe this part could be removed completely
  context.slot.payload_len = 0;
  context.slot.buffer = NULL;

  uint32_t ts_tx_4ns = sfd_time_4ns - context.tx_antenna_delay_4ns;
  dwt_writetxfctrl(3, 0, 0);
  //dwt_writetxdata(3, buf, 0);
  dwt_setdelayedtrxtime(ts_tx_4ns);
  /* errata TX-1: ensure TX done is issued */
  dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
  int res = dwt_starttx(DWT_START_TX_DELAYED);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_tx(sfd_time_4ns));
    STATETIME_MONITOR(valid_deadline=false);
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

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
    STATETIME_MONITOR(valid_deadline=true);
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
    STATETIME_MONITOR(valid_deadline=true; rxrfto_deadline=deadline_4ns; rxpto_deadline=ts_rx_4ns+context.rx_slot_preambleto_duration_4ns);
  }

  WARNIF(res!=DWT_SUCCESS);
  return res;
}

int trexd_rx_slot_fp(uint32_t expected_sfd_time_4ns, uint32_t deadline_4ns) {
  WARNIF(context.state != TREXD_ST_IDLE);
  context.state = TREXD_ST_FP_ENABLED;

  dwt_configure_sfdTo(1);

  DBG("RX at %lu till %lu now %lu", expected_sfd_time_4ns, deadline_4ns, dwt_readsystimestamphi32());
  // sanity check that the deadline is after the sfd time
  WARNIF(!time32_lt(expected_sfd_time_4ns, deadline_4ns));

  context.slot.buffer = NULL;
  // schedule delayed reception with timeout
  uint32_t ts_rx_4ns = expected_sfd_time_4ns - context.preamble_duration_4ns;
  deadline_4ns = deadline_4ns - context.preamble_duration_4ns;

#if FS_DEBUG
  rx_start = ts_rx_4ns;
#endif

  dwt_setdelayedtrxtime(ts_rx_4ns);
  dwt_setrxtimeout(DTU_4NS_TO_UUS(deadline_4ns - ts_rx_4ns));
  dwt_setpreambledetecttimeout(0);

  // Prepare send for later
  dwt_writetxfctrl(3, 0, 0);

  int res = dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR);
  if (res != DWT_SUCCESS)
    context.state = TREXD_ST_IDLE;
  else {
    STATETIME_MONITOR(dw1000_statetime_schedule_rx(ts_rx_4ns));
    STATETIME_MONITOR(rxrfto_deadline=deadline_4ns;valid_deadline=false);
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

    // NOTE: This should be possible but is not implemented as there is no associated TSM action, so there was no way to test it
    STATETIME_MONITOR(valid_deadline=false);
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
    STATETIME_MONITOR(valid_deadline=false);
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
    STATETIME_MONITOR(valid_deadline=false);
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
  tm_init();
  uint16_t rx_ant_dly, tx_ant_dly;
  // Make sure the radio is off
  dwt_forcetrxoff();
  context.state = TREXD_ST_IDLE;

  // Set interrupt handlers
  dw1000_set_isr(custom_dwt_isr);
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

void trexd_set_rx_slot_preambleto_pacs(const uint16_t preambleto_pacs, const uint32_t preambleto_duration)
{
    // NOTE: if set to 0 disables preamble timeout
    context.rx_slot_preambleto_pacs = preambleto_pacs;
    context.rx_slot_preambleto_duration_4ns = preambleto_duration;
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

static inline void epoch_start_isr(void) {
  tm_set_actual_epoch_start_mcu();

  uint32_t status = pdw1000local->cbData.status = dwt_read32bitreg(SYS_STATUS_ID); // Read status register low 32bits

  if (status & SYS_STATUS_RXRFTO) {
    dw1000_set_isr(custom_dwt_isr);

    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXRFTO); // Clear RX timeout event bits

    pdw1000local->wait4resp = 0;

    // Because of an issue with receiver restart after error conditions, an RX reset must be applied after any error or timeout event to ensure
    // the next good frame's timestamp is computed correctly.
    // See section "RX Message timestamp" in DW1000 User Manual.
    dwt_rxreset();
    dwt_forcetrxoff();

    context.state = TREXD_ST_IDLE;
    context.slot.status = TREX_NONE;

    slot_event();
  } else if (status & SYS_STATUS_ALL_RX_GOOD) {
    ERR("Gf %lu", status);

    dw1000_set_isr(custom_dwt_isr);

    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD); // Clear RX timeout event bits

    pdw1000local->wait4resp = 0;

    // Because of an issue with receiver restart after error conditions, an RX reset must be applied after any error or timeout event to ensure
    // the next good frame's timestamp is computed correctly.
    // See section "RX Message timestamp" in DW1000 User Manual.
    dwt_rxreset();
    dwt_forcetrxoff();

    context.state = TREXD_ST_IDLE;
    context.slot.status = TREX_NONE;

    slot_event();
  } else {
    DBG("Issue with epoch_start_isr %lu", status);
  }
}

#if FS_DEBUG

#ifndef FS_PER_EPOCH
#define FS_PER_EPOCH 100

#endif

static uint8_t fs_debug_log_counter = 0;
struct fs_debug_log_t {
  uint32_t logging_context;
  uint32_t duration_ns;
  bool bad_frame;
} fs_debug_log[FS_PER_EPOCH];

static void fs_debug_log_add(uint32 logging_context, uint32_t duration_ns, bool bad_frame) {
  if (fs_debug_log_counter >= FS_PER_EPOCH) {
    WARN("Trying to insert more entries in fs_debug_log then possible");
  } else {
    struct fs_debug_log_t to_add = {
      .logging_context = logging_context,
      .duration_ns = duration_ns,
      .bad_frame = bad_frame
    };
    fs_debug_log[fs_debug_log_counter] = to_add;
    ++fs_debug_log_counter;
  }
}

static void fs_debug_log_reset(){
  fs_debug_log_counter = 0;
}

void fs_debug_log_print() {
  uint8_t i = 0;
  while(i < fs_debug_log_counter) {
    struct fs_debug_log_t curr = fs_debug_log[i];

    uint64_t val = 0;

    // Use bitfield-like structure to create less output
    val |= (((uint64_t) (curr.logging_context & 0x1ffffff)) << 23);
    val |= ((curr.bad_frame & 0x1) << 22);
    val |= (curr.duration_ns & 0x3fffff);

    PRINT("k%012" PRIx64, val);

    ++i;
  }
}

#define FS_DEBUG_MONITOR(...) __VA_ARGS__
#else
#define FS_DEBUG_MONITOR(...) do {} while(0)
#endif

int trexd_pre_epoch_procedure(uint32_t actual_epoch_start) {
  // Reset the information to calcualte the ratio between the clock ofthe mcu and radio as we are changing epoch (there could be overflows or the radio could be in the future put to a lower state)
  tm_reset();

  // Set the time at which we expect the timer to fire
  tm_set_actual_epoch_start_dw1000(actual_epoch_start);

  // Set a special isr to handle use of the timer to map the radio and mcu timer counter
  dw1000_set_isr(epoch_start_isr);
  FS_DEBUG_MONITOR(fs_debug_log_reset());

  // Set the timer
  return trexd_set_timer(actual_epoch_start);
}

static inline void custom_dwt_isr(void) {
    // NOTE: This is a modified version of dwt_isr present in deca_device.c

    // As first thing, get the mcu timer counter so that is not influenced by the time of the rest of the operations
    tm_set_timestamp_mcu();

    uint32 status = pdw1000local->cbData.status = dwt_read32bitreg(SYS_STATUS_ID); // Read status register low 32bits

    // Handle TX confirmation event
    if(status & SYS_STATUS_TXFRS)
    {
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX); // Clear TX event bits

        // In the case where this TXFRS interrupt is due to the automatic transmission of an ACK solicited by a response (with ACK request bit set)
        // that we receive through using wait4resp to a previous TX (and assuming that the IRQ processing of that TX has already been handled), then
        // we need to handle the IC issue which turns on the RX again in this situation (i.e. because it is wrongly applying the wait4resp after the
        // ACK TX).
        // See section "Transmit and automatically wait for response" in DW1000 User Manual
        if((status & SYS_STATUS_AAT) && pdw1000local->wait4resp)
        {
            dwt_forcetrxoff(); // Turn the RX off
            dwt_rxreset(); // Reset in case we were late and a frame was already being received
        }

        // Call the corresponding callback if present
        if(pdw1000local->cbTxDone != NULL)
        {
            pdw1000local->cbTxDone(&pdw1000local->cbData);
        }
    } else

    // Handle RX good frame event
    if(status & SYS_STATUS_RXFCG)
    {
        uint16 finfo16;
        uint16 len;

        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD); // Clear all receive status bits

        pdw1000local->cbData.rx_flags = 0;

        // Read frame info - Only the first two bytes of the register are used here.
        finfo16 = dwt_read16bitoffsetreg(RX_FINFO_ID, RX_FINFO_OFFSET);

        // Report frame length - Standard frame length up to 127, extended frame length up to 1023 bytes
        len = finfo16 & RX_FINFO_RXFL_MASK_1023;
        if(pdw1000local->longFrames == 0)
        {
            len &= RX_FINFO_RXFLEN_MASK;
        }
        pdw1000local->cbData.datalength = len;

        // Report ranging bit
        if(finfo16 & RX_FINFO_RNG)
        {
            pdw1000local->cbData.rx_flags |= DWT_CB_DATA_RX_FLAG_RNG;
        }

        // Report frame control - First bytes of the received frame.
        dwt_readfromdevice(RX_BUFFER_ID, 0, FCTRL_LEN_MAX, pdw1000local->cbData.fctrl);

        // Because of a previous frame not being received properly, AAT bit can be set upon the proper reception of a frame not requesting for
        // acknowledgement (ACK frame is not actually sent though). If the AAT bit is set, check ACK request bit in frame control to confirm (this
        // implementation works only for IEEE802.15.4-2011 compliant frames).
        // This issue is not documented at the time of writing this code. It should be in next release of DW1000 User Manual (v2.09, from July 2016).
        if((status & SYS_STATUS_AAT) && ((pdw1000local->cbData.fctrl[0] & FCTRL_ACK_REQ_MASK) == 0))
        {
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_AAT); // Clear AAT status bit in register
            pdw1000local->cbData.status &= ~SYS_STATUS_AAT; // Clear AAT status bit in callback data register copy
            pdw1000local->wait4resp = 0;
        }

        // Call the corresponding callback if present
        if(pdw1000local->cbRxOk != NULL)
        {
            pdw1000local->cbRxOk(&pdw1000local->cbData);
        }

        if (pdw1000local->dblbuffon)
        {
            // Toggle the Host side Receive Buffer Pointer
            dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_HRBT_OFFSET, 1);
        }
    } else

    // Handle frame reception/preamble detect timeout events
    if(status & SYS_STATUS_ALL_RX_TO)
    {
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXRFTO); // Clear RX timeout event bits

        if ((status & SYS_STATUS_RXRFTO) && (context.state == TREXD_ST_FP_SENT || context.state == TREXD_ST_FP_SENT_DETECTED)) {
          // Do nothing
          PRINT("Ignoring double call");
        } else {
            pdw1000local->wait4resp = 0;

            complete_status = dwt_read32bitoffsetreg(SYS_STATUS_ID, 0x04);
            complete_status <<= 32;
            complete_status |= status;

            // Because of an issue with receiver restart after error conditions, an RX reset must be applied after any error or timeout event to ensure
            // the next good frame's timestamp is computed correctly.
            // See section "RX Message timestamp" in DW1000 User Manual.
            dwt_forcetrxoff();
            dwt_rxreset();

            // Call the corresponding callback if present
            if(pdw1000local->cbRxTo != NULL)
            {
              if (complete_status & SYS_STATUS_RXPREJ)
                statetime_tracking = false;

              pdw1000local->cbRxTo(&pdw1000local->cbData);
            }
        }
    } else

    // Handle RX errors events
    if(status & SYS_STATUS_ALL_RX_ERR)
    {
        pdw1000local->wait4resp = 0;

        if (context.state == TREXD_ST_FP_ENABLED) {
          /* It seems this has to be done immediately as otherwise the radio has enough time to
           * raise another interrupt for which a possible race condition can happen.
           * In particular after RXPHE if we set it later the radio has enough time to raise 
           * another interrupt for RXFCE and this seems to create an issue for which status is overwritten
           * and we and up in the "Unexpected error in fs" print evne if we have RX
           */
          if (status & SYS_STATUS_RXSFDTO) {
            context.state = TREXD_ST_FP_SENT;
            trexd_tx_fp();

            FS_DEBUG_MONITOR(fs_debug_log_add(logging_context, tm_get_elapsed_time_ns(rx_start), false));

            // Busy-wait until the radio completed the transmission
            while(!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS));

            // Reset the part of the status used
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX | SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits

            // Call the corresponding callback if present
            if(pdw1000local->cbTxDone != NULL)
            {
                pdw1000local->cbTxDone(&pdw1000local->cbData);
            }
          } else if(status & (SYS_STATUS_RXPHE | SYS_STATUS_RXFCE | SYS_STATUS_RXRFSL)) {
            /* We consider these detecting a preamble as in these cases we received:
             *  - an invalid PHY header (PHE = PHY Header error)
             *  - an invalid FCS (a frame CRC checking error)
             *  - an invalid data part (RFSL = Reed Solomon Frame Sync Loss meaning an non-correctable error during the decoding of the data the data portion of the frame)
             * meaning that we received (with high probability due the low amount of false positives) a preamble but we received some invalid part.
             * We could decide to either re-propagate this (but we might be very late in doing this) 
             * or if there is too much risk that this happens we could do it anyways (it could for example be useful for a bottleneck node)
             */
            context.state = TREXD_ST_FP_SENT_DETECTED;
            trexd_tx_fp();

            FS_DEBUG_MONITOR(fs_debug_log_add(logging_context, tm_get_elapsed_time_ns(rx_start),true));

            // Busy-wait until the radio completed the transmission
            while(!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS));

            // Reset the part of the status used
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX | SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits

            // Call the corresponding callback if present
            if(pdw1000local->cbTxDone != NULL)
            {
                pdw1000local->cbTxDone(&pdw1000local->cbData);
            }
          } else {
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits
            ERR("Unexpected error in fp %"PRIu32, status);
            context.slot.status = TREX_FS_ERROR;

            update_rxerr_stats(status);

            dwt_forcetrxoff();
            dwt_rxreset();

            dwt_configure_sfdTo(DW1000_CONF_SFD_TIMEOUT);
            context.slot.radio_status = status;
            context.state = TREXD_ST_IDLE;

            STATETIME_MONITOR(dw1000_statetime_after_rxerr(dwt_readsystimestamphi32()));

            // We skip calling cbRxErr as that would make it behave like an actual error happened (i.e. reporting the error and increasing the error stats)
            // but directly call the callback to the application
            slot_event();
          }
        } else if (context.state == TREXD_ST_FP_SENT || context.state == TREXD_ST_FP_SENT_DETECTED) {
          dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits
          PRINT("double call rxErr");
        } else {
          dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR); // Clear RX error event bits

          // Because of an issue with receiver restart after error conditions, an RX reset must be applied after any error or timeout event to ensure
          // the next good frame's timestamp is computed correctly.
          // See section "RX Message timestamp" in DW1000 User Manual.
          dwt_forcetrxoff();
          dwt_rxreset();

          // Call the corresponding callback if present
          if(pdw1000local->cbRxErr != NULL)
          {
            pdw1000local->cbRxErr(&pdw1000local->cbData);
          }
        }
    }
}
