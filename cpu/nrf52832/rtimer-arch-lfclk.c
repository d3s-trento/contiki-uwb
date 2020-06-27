/*
 * Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \addtogroup nrf52832
 * @{
 *
 * \file
 *      Implementation of the architecture dependent rtimer functions for the nRF52
 */
#include "dwm1001-dev-board.h"
/*---------------------------------------------------------------------------*/
#include "nrf.h"
#include "nrfx_rtc.h"
#include "app_error.h"
/*---------------------------------------------------------------------------*/
#include "contiki.h"
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

static const nrfx_rtc_t rtc   = NRFX_RTC_INSTANCE(PLATFORM_RTIMER_RTC_INSTANCE_ID); /**< RTC instance used for platform rtimer */
static nrfx_rtc_config_t app_rtc_config = NRFX_RTC_DEFAULT_CONFIG;

/**
 * \brief Handler for timer events.
 *
 * \param event_type type of an event that should be handled
 * \param p_context opaque data pointer passed from nrfx_timer_init()
 */
static void
rtc_handler(nrfx_rtc_int_type_t int_type)
{
  switch (int_type) {
    case NRFX_RTC_INT_COMPARE0:
      rtimer_run_next();
      break;

    default:
      //Do nothing.
      break;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize platform rtimer
 */
void
rtimer_arch_init(void)
{
  app_rtc_config.prescaler = RTC_FREQ_TO_PRESCALER(32768);
  
  ret_code_t err_code = nrfx_rtc_init(&rtc, &app_rtc_config, rtc_handler);
  APP_ERROR_CHECK(err_code);
  nrfx_rtc_enable(&rtc);

  // we assume that LFCLK is initialised and started in clock.c
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Schedules an rtimer task to be triggered at time t
 * \param t The time when the task will need executed.
 *
 * \e t is an absolute time, in other words the task will be executed AT
 * time \e t, not IN \e t rtimer ticks.
 *
 * This function schedules a one-shot event with the nRF RTC.
 */
void
rtimer_arch_schedule(rtimer_clock_t t)
{
/* From nRF docs:
 * The driver is not entering a critical section when configuring RTC, which
 * means that it can be preempted for a certain amount of time. When the driver
 * was preempted and the value to be set is short in time, there is a risk that
 * the driver sets a compare value that is behind. In this case, if the
 * reliable mode is enabled for the specified instance, the risk is handled.
 * However, to detect if the requested value is behind, this mode makes the
 * following assumptions: 
 * -  The maximum preemption time in ticks (8-bit value)
 *    is known and is less than 7.7 ms (for prescaler = 0, RTC frequency 32 kHz).
 * -  The requested absolute compare value is not bigger than
 *    (0x00FFFFFF)-tick_latency. It is the user's responsibility to ensure
 *    this.
 */
  /*nrfx_err_t err =*/ nrfx_rtc_cc_set(&rtc, 0, t & 0x00FFFFFF, true);
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Returns the current real-time clock time
 * \return The current rtimer time in ticks
 *
 */
rtimer_clock_t
rtimer_arch_now()
{
  return nrfx_rtc_counter_get(&rtc);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
