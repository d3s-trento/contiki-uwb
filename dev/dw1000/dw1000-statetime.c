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

/*
 * \file Radio State Monitor module (aka statetime)
 *
 * \author
 *      Diego Lobba <diego.lobba@gmail.com>
 */

#include PROJECT_CONF_H

#include "dw1000-statetime.h"
#include "dw1000-util.h"
#include "dw1000-conv.h"
#include "dw1000-config.h"
#include "evb1000-timer-mapping.h"
#include <inttypes.h>
#include <stdbool.h>
#include "print-def.h"

#define LOG_PREFIX "st"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#pragma message STRDEF(SNIFF_FS)
#pragma message STRDEF(SNIFF_FS_OFF_TIME)

bool statetime_tracking = false;

/*---------------------------------------------------------------------------*/
#define DEBUG           1
#if DEBUG
#include <stdio.h>
#define PRINTF(...)     printf(__VA_ARGS__)
#else
#define PRINTF(...)     do {} while(0)
#endif

#define ENERGY_LOG(...) PRINTF("[ENERGY]" __VA_ARGS__)

#define TIME_LT32(T1, T2) ((int32_t)(T2-T1) >= 0)

// the schedule can shift of +-4 ns due to the scheduling precision.
#define STATETIME_SFD_SLACK_4NS     2 // 8ns

#if DEBUG
#define STATETIME_DBG(...) __VA_ARGS__
static bool restarted; // used when debugging
#else
#define STATETIME_DBG(...) do {} while(0);
#endif


/*---------------------------------------------------------------------------*/
/*                          STATIC VARIABLES                                 */
/*---------------------------------------------------------------------------*/
static dw1000_statetime_context_t context;
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_context_init()
{
    context.state = DW1000_IDLE;

    context.idle_time_us = 0;
    context.rx_preamble_hunting_time_us = 0;
    context.rx_preamble_time_us = 0;
    context.rx_data_time_us = 0;
    context.tx_preamble_time_us = 0;
    context.tx_data_time_us = 0;

    context.is_restarted = true;
    context.tracing = false;
    context.last_idle_32hi = 0;
    context.is_rx_after_tx = 0;
    context.rx_delay_32hi = 0;
    context.schedule_32hi = 0;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_set_last_idle(const uint32_t ts_idle_32hi)
{
    if (!context.tracing) return;

    context.last_idle_32hi = ts_idle_32hi;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_tx(const uint32_t schedule_tx_32hi)
{
    if (!context.tracing) return;

    context.is_rx_after_tx = false;
    context.schedule_32hi  = schedule_tx_32hi;
    context.state = DW1000_SCHEDULED_TX;

}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_txrx(const uint32_t schedule_tx_32hi, const uint32_t rx_delay_uus)
{
    if (!context.tracing) return;

    context.is_rx_after_tx = true;
    context.schedule_32hi  = schedule_tx_32hi;
    context.rx_delay_32hi  = rx_delay_uus * 1000 / DWT_TICK_TO_NS_32;
    context.state = DW1000_SCHEDULED_TX;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_rx(const uint32_t schedule_rx_32hi)
{
    if (!context.tracing) return;

    context.is_rx_after_tx = false;
    context.schedule_32hi = schedule_rx_32hi;
    context.state = DW1000_SCHEDULED_RX;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_tx(const uint32_t sfd_tx_32hi, const uint16_t framelength)
{
    STATETIME_DBG(restarted = context.is_restarted);
    if (!context.tracing) return;

    WARNIF(context.state != DW1000_SCHEDULED_TX);

    uint32_t preamble_time_ns = estimate_preamble_time_ns();
    uint32_t payload_time_ns  = estimate_payload_time_ns(framelength);

    if (context.is_restarted) {
        // first transmission of the epoch. last idle is the time at the beginning
        // of preamble transmission.
        // NOTE: schedule reports the expected sfd time
        dw1000_statetime_set_last_idle(sfd_tx_32hi - STATETIME_SFD_SLACK_4NS - (preamble_time_ns / DWT_TICK_TO_NS_32));
        context.is_restarted = false;
    }

    uint32_t idle_sfd_time_ns = (sfd_tx_32hi - context.last_idle_32hi) * DWT_TICK_TO_NS_32;
    WARNIF(!TIME_LT32(context.last_idle_32hi, sfd_tx_32hi));
    WARNIF(!TIME_LT32(preamble_time_ns, idle_sfd_time_ns));
    STATETIME_DBG(
    if (!TIME_LT32(preamble_time_ns, idle_sfd_time_ns) || !TIME_LT32(preamble_time_ns, idle_sfd_time_ns)) {
        PRINTF("S %lu, SFD %lu, R %d\n", context.schedule_32hi, sfd_tx_32hi, restarted);
        PRINTF("P %lu, R %lu\n", preamble_time_ns, idle_sfd_time_ns);
    })
    // by definition the time between the last idle time and the sfd cannot
    // be smaller than the preamble
    context.idle_time_us += (idle_sfd_time_ns - preamble_time_ns) / 1000;
    context.tx_preamble_time_us += preamble_time_ns / 1000;
    context.tx_data_time_us += payload_time_ns / 1000;

    // the radio goes idle when tx done is issued, therefore
    // roughly at sfd + the time required to read the PHY payload
    context.last_idle_32hi = sfd_tx_32hi + payload_time_ns / DWT_TICK_TO_NS_32;

    if (context.is_rx_after_tx) {

        // setrxaftertx_delay function used
        context.schedule_32hi = context.last_idle_32hi + context.rx_delay_32hi;
        context.state = DW1000_SCHEDULED_RX;
        context.rx_delay_32hi = 0;
        context.is_rx_after_tx = false;

    } else {
        context.state = DW1000_IDLE;
    }
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_rxerr(const uint32_t now_32hi)
{
    STATETIME_DBG(restarted = context.is_restarted);
    if (!context.tracing) return;

    WARNIF(context.state != DW1000_SCHEDULED_RX);

    if (context.is_restarted) {
        // start counting when the radio was turned on
        dw1000_statetime_set_last_idle(context.schedule_32hi);
        context.is_restarted = false;
    }

    uint32_t p_time_ns = 0;

    if (context.state == DW1000_SCHEDULED_RX) {

        // using the rx_enable function
        // context.schedule_32hi stores the timestamp the
        // radio switched to rx
        WARNIF(context.schedule_32hi == 0);
        WARNIF(!TIME_LT32(context.schedule_32hi, now_32hi));
        WARNIF(!TIME_LT32(context.last_idle_32hi, context.schedule_32hi));

        STATETIME_DBG(
        if (!TIME_LT32(context.schedule_32hi, now_32hi) || !TIME_LT32(context.last_idle_32hi, context.schedule_32hi)) {
            printf("LI %lu S %lu, N %lu, R %d\n", context.last_idle_32hi, context.schedule_32hi, now_32hi, restarted);
        })

        p_time_ns = tm_get_elapsed_time_ns(context.schedule_32hi);

        if (statetime_tracking) {
          printf("T %lu %lu\n", logging_context, p_time_ns);
        }

        context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
        context.rx_preamble_hunting_time_us += p_time_ns / 1000;
    }

    // radio switched to idle when rx_ok callback was issued
    context.is_rx_after_tx = false;
    context.rx_delay_32hi  = 0;
    context.last_idle_32hi = context.schedule_32hi + p_time_ns / DWT_TICK_TO_NS_32; // TODO: Da ricontrollare

    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_rx(const uint32_t sfd_rx_32hi, const uint16_t framelength)
{
    STATETIME_DBG(restarted = context.is_restarted);
    if (!context.tracing) return;

    WARNIF(context.state != DW1000_SCHEDULED_RX);

    if (context.is_restarted) {
        // start counting when the radio was turned on
        dw1000_statetime_set_last_idle(context.schedule_32hi);
        context.is_restarted = false;
    }

    uint32_t preamble_time_ns = estimate_preamble_time_ns();
    uint32_t payload_time_ns  = estimate_payload_time_ns(framelength);

    if (context.state == DW1000_SCHEDULED_RX) {

        uint32_t schedule_sfd_time_ns = 0;
        uint32_t ph_time_ns = 0;
        // using the rx_enable function
        // context.schedule_32hi stores the timestamp the
        // radio switched to rx
        WARNIF(context.schedule_32hi == 0);
        WARNIF(!TIME_LT32(context.schedule_32hi, sfd_rx_32hi));
        WARNIF(!TIME_LT32(context.last_idle_32hi, context.schedule_32hi));
        schedule_sfd_time_ns = (sfd_rx_32hi - context.schedule_32hi) * DWT_TICK_TO_NS_32;
        STATETIME_DBG(
        if (!TIME_LT32(context.schedule_32hi, sfd_rx_32hi) || !TIME_LT32(context.last_idle_32hi, context.schedule_32hi)) {
            printf("LI %lu S %lu, SFD %lu, R %d\n", context.last_idle_32hi, context.schedule_32hi, sfd_rx_32hi, restarted);
        })

        // the radio can wake up and manage to sucessfully receive a packet despite
        // not having received the entire preamble.
        // If this is the case, consider the time schedule_rx <-> sfd as
        // preamble_time
        if (TIME_LT32(preamble_time_ns, schedule_sfd_time_ns)) {
            ph_time_ns = schedule_sfd_time_ns - preamble_time_ns;
        } else  {
            preamble_time_ns = schedule_sfd_time_ns;
            ph_time_ns = 0;
        }
        context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi) * DWT_TICK_TO_NS_32 / 1000;
        context.rx_preamble_hunting_time_us += ph_time_ns / 1000;

    } 
    context.rx_preamble_time_us += preamble_time_ns / 1000;
    context.rx_data_time_us += payload_time_ns / 1000;

    // radio switched to idle when rx_ok callback was issued
    context.last_idle_32hi = sfd_rx_32hi + payload_time_ns / DWT_TICK_TO_NS_32;

    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_fs_pos(const uint32_t sfd_tx_32hi, const uint16_t framelength, bool sfdto)
{ // TODO: Da usare tempo cpu anche qui?
    STATETIME_DBG(restarted = context.is_restarted);
    if (!context.tracing) return;

    WARNIF(context.state != DW1000_SCHEDULED_RX);

    uint32_t preamble_time_ns = estimate_preamble_time_ns();
    uint32_t payload_time_ns  = estimate_payload_time_ns(framelength);

    if (context.is_restarted) {
        // first transmission of the epoch. last idle is the time at the beginning
        // of preamble transmission.
        // NOTE: schedule reports the expected sfd time
        dw1000_statetime_set_last_idle(context.schedule_32hi);// sfd_tx_32hi - STATETIME_SFD_SLACK_4NS - (preamble_time_ns / DWT_TICK_TO_NS_32));
        context.is_restarted = false;
    }

    // The last time we were in idle must be smaller or equal then the last time we scheduled
    WARNIF(!(TIME_LT32(context.last_idle_32hi, context.schedule_32hi) || context.last_idle_32hi == context.schedule_32hi));
    // As we first schedule an rx and then do the rx the sfd of the rx must be after the schedule
    WARNIF(!TIME_LT32(context.schedule_32hi, sfd_tx_32hi));

#if STATETIME_CONF_ON && (DW1000_CONF_PLEN != DWT_PLEN_64 || DW1000_CONF_PRF != DWT_PRF_64M || DW1000_CONF_PAC != DWT_PAC8)
#error "Statetime + Flick with PLEN != 64 not supported"
#endif

    uint32_t p_time_ns = ((sfd_tx_32hi - context.schedule_32hi) * DWT_TICK_TO_NS_32 - preamble_time_ns);

#if SNIFF_FS
    uint32_t pr_time_ns;

    if (sfdto) {
      WARNIF(p_time_ns < 9 * PRE_SYM_PRF64_TO_DWT_TIME_32);

      pr_time_ns = 9 * PRE_SYM_PRF64_TO_DWT_TIME_32;
    } else {
      pr_time_ns = MIN(16 * PRE_SYM_PRF64_TO_DWT_TIME_32, p_time_ns);
    }

    uint32_t ph_time_ns = p_time_ns - pr_time_ns;

    context.rx_preamble_time_us += pr_time_ns / 1000;


    context.rx_preamble_hunting_time_us += (ph_time_ns*(16 * PRE_SYM_PRF64_TO_DWT_TIME_32)/(16 * PRE_SYM_PRF64_TO_DWT_TIME_32 + SNIFF_FS_OFF_TIME * UUS_TO_DWT_TIME_32)) / 1000;
    context.idle_time_us                += (ph_time_ns - (ph_time_ns*(16 * PRE_SYM_PRF64_TO_DWT_TIME_32)/(16 * PRE_SYM_PRF64_TO_DWT_TIME_32 + SNIFF_FS_OFF_TIME * UUS_TO_DWT_TIME_32)) ) / 1000;
#else
    context.rx_preamble_hunting_time_us += p_time_ns / 1000;
#endif

    context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi) * DWT_TICK_TO_NS_32 / 1000;
    context.tx_preamble_time_us += preamble_time_ns / 1000;
    context.tx_data_time_us += payload_time_ns / 1000;

    // the radio goes idle when tx done is issued, therefore
    // roughly at sfd + the time required to read the PHY payload
    context.last_idle_32hi = sfd_tx_32hi + payload_time_ns / DWT_TICK_TO_NS_32;

    // TODO: Check again this last part
    if (context.is_rx_after_tx) {

        // setrxaftertx_delay function used
        context.schedule_32hi = context.last_idle_32hi + context.rx_delay_32hi;
        context.state = DW1000_SCHEDULED_RX;
        context.rx_delay_32hi = 0;
        context.is_rx_after_tx = false;

    } else {
        context.state = DW1000_IDLE;
    }
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_fs_neg(const uint32_t now_32hi)
{
    STATETIME_DBG(restarted = context.is_restarted);
    if (!context.tracing) return;

    WARNIF(context.state != DW1000_SCHEDULED_RX);

    if (context.is_restarted) {
        // start counting when the radio was turned on
        dw1000_statetime_set_last_idle(context.schedule_32hi);
        context.is_restarted = false;
    }

    uint32_t p_time_ns = 0;

    if (context.state == DW1000_SCHEDULED_RX) {

        // using the rx_enable function
        // context.schedule_32hi stores the timestamp the
        // radio switched to rx
        WARNIF(context.schedule_32hi == 0);
        WARNIF(!TIME_LT32(context.schedule_32hi, now_32hi));
        WARNIF(!TIME_LT32(context.last_idle_32hi, context.schedule_32hi));

        STATETIME_DBG(
        if (!TIME_LT32(context.schedule_32hi, now_32hi) || !TIME_LT32(context.last_idle_32hi, context.schedule_32hi)) {
            printf("S %lu, N %lu, R %d\n", context.schedule_32hi, now_32hi, restarted);
        })

        p_time_ns = tm_get_elapsed_time_ns(context.schedule_32hi);

        if (statetime_tracking) {
          printf("T %lu %lu\n", logging_context, p_time_ns);
        }

#if SNIFF_FS
        context.rx_preamble_hunting_time_us += (p_time_ns * (16 * PRE_SYM_PRF64_TO_DWT_TIME_32)/(16 * PRE_SYM_PRF64_TO_DWT_TIME_32 + SNIFF_FS_OFF_TIME * UUS_TO_DWT_TIME_32)) / 1000;
        context.idle_time_us                += (p_time_ns - (p_time_ns * (16 * PRE_SYM_PRF64_TO_DWT_TIME_32)/(16 * PRE_SYM_PRF64_TO_DWT_TIME_32 + SNIFF_FS_OFF_TIME * UUS_TO_DWT_TIME_32)) ) / 1000;
#else
        context.rx_preamble_hunting_time_us += p_time_ns / 1000;
#endif

        context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
    }

    // radio switched to idle when rx_ok callback was issued
    context.is_rx_after_tx = false;
    context.rx_delay_32hi  = 0;
    context.last_idle_32hi = context.schedule_32hi + p_time_ns / DWT_TICK_TO_NS_32; // TODO: Da ricontrollare

    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_start()
{
    context.tracing = true;
    context.is_restarted = true;
    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_abort(const uint32_t now_32hi)
{
    if (!context.tracing) return;

    if (context.is_restarted) {

        if (context.state == DW1000_SCHEDULED_RX || context.state == DW1000_SCHEDULED_TX) {
            // start counting when the radio was turned on
            dw1000_statetime_set_last_idle(context.schedule_32hi);
            context.is_restarted = false;
        }
        else {
            // there is literally no time reference.
            // no action was scheduled. We assume this scenario to be rare
            // and therefore consider this time negligible. We therefore
            // set last_idle to now_32hi so that each timing will be 0.
            dw1000_statetime_set_last_idle(now_32hi);
            context.is_restarted = false;
        }
    }

    if (context.state == DW1000_SCHEDULED_RX) {

        // using the rx_enable function
        // context.schedule_32hi stores the timestamp the
        // radio switched to rx
        WARNIF(context.schedule_32hi == 0);
        WARNIF(!TIME_LT32(context.last_idle_32hi, context.schedule_32hi));
        if (TIME_LT32(context.schedule_32hi, now_32hi)) {
            context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
            context.rx_preamble_hunting_time_us += (now_32hi - context.schedule_32hi) * DWT_TICK_TO_NS_32 / 1000;
        }
        else {
            // radio operation interrupt before the radio was turned on.
            // this time is spent in idle
            context.idle_time_us += (now_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
        }
    }
    else if (context.state == DW1000_SCHEDULED_TX) {
        // consider this time to be spend TX the preamble
        WARNIF(context.schedule_32hi == 0);
        WARNIF(!TIME_LT32(context.last_idle_32hi, context.schedule_32hi));

        if (TIME_LT32(context.schedule_32hi, now_32hi)) {
            context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
            context.tx_preamble_time_us += (now_32hi - context.schedule_32hi) * DWT_TICK_TO_NS_32 / 1000;
        }
        else {
            // radio interrupted before being active.
            context.idle_time_us += (now_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
        }
    }
    else if (context.state == DW1000_IDLE){
        WARNIF(!TIME_LT32(context.last_idle_32hi, now_32hi));
        context.idle_time_us += (now_32hi - context.last_idle_32hi)  * DWT_TICK_TO_NS_32 / 1000;
    }
    else {
        // Error?
    }

    // radio switched to idle when rx_ok callback was issued
    context.is_rx_after_tx = false;
    context.rx_delay_32hi = 0;
    context.last_idle_32hi = now_32hi;

    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_stop(void)
{
    context.state = DW1000_IDLE;
    context.tracing = false;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_stop_at(uint32_t end_32hi)
{
  WARNIF(!context.tracing);
  WARNIF(context.state != DW1000_IDLE);
  WARNIF(context.is_restarted);

  dw1000_statetime_stop();

  context.idle_time_us += (end_32hi - context.last_idle_32hi) * DWT_TICK_TO_NS_32 / 1000;
}
/*---------------------------------------------------------------------------*/
dw1000_statetime_context_t*
dw1000_statetime_get_context()
{
    return &context;
}
/*---------------------------------------------------------------------------*/
/*                              UTILITY FUNCTIONS                            */
/*---------------------------------------------------------------------------*/
uint32_t estimate_preamble_time_ns()
{
    return dw1000_estimate_tx_time(dw1000_get_current_cfg(), 0, true);
}
/*---------------------------------------------------------------------------*/
uint32_t estimate_payload_time_ns(const uint16_t framelength)
{
    return dw1000_estimate_tx_time(dw1000_get_current_cfg(), framelength, false) -
        dw1000_estimate_tx_time(dw1000_get_current_cfg(), 0, true);
}
/*---------------------------------------------------------------------------*/
uint32_t dw1000_statetime_get_schedule_32hi(){
  return context.schedule_32hi;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_print()
{
    PRINTF("E %lu, I %"PRIu64", TP %"PRIu64", TD %"PRIu64", RH %"PRIu64", RP %"PRIu64", RD %"PRIu64"\n",
            logging_context,
            context.idle_time_us, context.tx_preamble_time_us, context.tx_data_time_us,
            context.rx_preamble_hunting_time_us, context.rx_preamble_time_us, context.rx_data_time_us);
}

void
dw1000_statetime_log(dw1000_statetime_log_t* entry)
{
    entry -> idle_time_us = context.idle_time_us;
    entry -> tx_preamble_time_us = context.tx_preamble_time_us;
    entry -> tx_data_time_us = context.tx_data_time_us;
    entry -> rx_preamble_hunting_time_us = context.rx_preamble_hunting_time_us;
    entry -> rx_preamble_time_us = context.rx_preamble_time_us;
    entry -> rx_data_time_us = context.rx_data_time_us;
}
