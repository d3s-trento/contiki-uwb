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
 * \addtogroup nrf52832-dev Device drivers
 * @{
 *
 * \addtogroup nrf52832-clock Clock driver
 * @{
 *
 * \file
 *         Software clock implementation for the nRF52.
 * \author
 *         Wojciech Bober <wojciech.bober@nordicsemi.no>
 *
 */
#include "contiki.h"
/*---------------------------------------------------------------------------*/
#include "nrf.h"
#include "nrf52832_peripherals.h"
#include "nrfx_rtc.h"
#include "nrfx_clock.h"
#include "nrf_delay.h"
#include "app_error.h"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------*/
#define NRF_DEBUG_PINS 0

#if NRF_DEBUG_PINS
#include "nrf_gpio.h"

#define PIN1 15 // pin 3 of the RPi header of DWM1001-DEV
#define PIN2 8  // pin 5 of the RPi header of DWM1001-DEV

#define PIN_INIT(x) nrf_gpio_cfg_output(x);
#define PIN_SET(x)  NRF_GPIO->OUTSET = (1 << x)
#define PIN_CLR(x)  NRF_GPIO->OUTCLR = (1 << x)
#define PIN_TGL(x)  nrf_gpio_pin_toggle(x)
#else
#define PIN_INIT(x)
#define PIN_SET(x)
#define PIN_CLR(x)
#define PIN_TGL(x)
#endif

/*---------------------------------------------------------------------------*/
/*static*/ const nrfx_rtc_t rtc   = NRFX_RTC_INSTANCE(PLATFORM_CLOCK_RTC_INSTANCE_ID); /**< RTC instance used for platform clock */
// APP_TIMER_DEF(timer_id);
/*---------------------------------------------------------------------------*/
static volatile uint32_t ticks;
/*---------------------------------------------------------------------------*/
static inline void
clock_update(void)
{
  // we sometimes miss this interrupt, so compensate the counter here
  static uint32_t previous = 0;
  uint32_t current = nrfx_rtc_counter_get(&rtc);
  ticks += (current - previous) % 0x1000000; // 24-bit counter
  previous = current;
  PIN_TGL(PIN1);
  if (etimer_pending()) {
    etimer_request_poll();
  }
}
/**
 * \brief Function for handling the RTC0 interrupts
 * \param int_type Type of interrupt to be handled
 */
static void
rtc_handler(nrfx_rtc_int_type_t int_type)
{
  if (int_type == NRFX_RTC_INT_TICK) {
    clock_update();
  }
}

#ifndef SOFTDEVICE_PRESENT
/* empty event handelr needed by nrfx_clock driver */
static void clock_event_handler(nrfx_clock_evt_type_t event){}

/** \brief Function starting the internal LFCLK XTAL oscillator.
 */
static void
lfclk_config(void)
{
  // TODO app_timer does not handle lfclk clock
  ret_code_t err_code = nrfx_clock_init(clock_event_handler);
  APP_ERROR_CHECK(err_code);
  nrfx_clock_enable();

  nrfx_clock_lfclk_start();
}
#endif

/**
 * \brief Function initialization and configuration of RTC driver instance.
 */
static void
rtc_config(void)
{

  //Initialize RTC instance
  // app_timer_init();
  ret_code_t err_code;
  // err_code = app_timer_create(&timer_id,
  //                             APP_TIMER_MODE_REPEATED,
  //                             rtc_handler);
  // APP_ERROR_CHECK(err_code);

  // err_code = app_timer_start(timer_id, APP_TIMER_TICKS(1), NULL);
  // APP_ERROR_CHECK(err_code);

  // Setup the RTC config
  static nrfx_rtc_config_t app_rtc_config;
  app_rtc_config.interrupt_priority = NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY;
  app_rtc_config.prescaler = RTC_FREQ_TO_PRESCALER(NRFX_RTC_DEFAULT_CONFIG_FREQUENCY);
  app_rtc_config.reliable = NRFX_RTC_DEFAULT_CONFIG_RELIABLE;
  app_rtc_config.tick_latency = NRFX_RTC_US_TO_TICKS(NRFX_RTC_MAXIMUM_LATENCY_US, NRFX_RTC_DEFAULT_CONFIG_FREQUENCY);

  err_code = nrfx_rtc_init(&rtc, &app_rtc_config, rtc_handler);
  APP_ERROR_CHECK(err_code);

  //Enable tick event & interrupt
  nrfx_rtc_tick_enable(&rtc, true);

  //Power on RTC instance
  nrfx_rtc_enable(&rtc);
}
/*---------------------------------------------------------------------------*/
void
clock_init(void)
{
  PIN_INIT(PIN1);
  ticks = 0;
#ifndef SOFTDEVICE_PRESENT
  lfclk_config();
#endif
  rtc_config();
}
/*---------------------------------------------------------------------------*/
CCIF clock_time_t
clock_time(void)
{
  return ticks;
  // We cannot use directly the following as the RTC register is 24-bit, 
  // which breaks Contiki's assumptions about the clock counter
  //return nrfx_rtc_counter_get(&rtc);
}
/*---------------------------------------------------------------------------*/
CCIF unsigned long
clock_seconds(void)
{
  return ticks/CLOCK_SECOND;
}
/*---------------------------------------------------------------------------*/
void
clock_wait(clock_time_t i)
{
  clock_time_t start;
  start = clock_time();
  while (clock_time() - start < (clock_time_t)i) {
    __WFE();
  }
}
/*---------------------------------------------------------------------------*/
void
clock_delay_usec(uint16_t dt)
{
  nrf_delay_us(dt);
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Obsolete delay function but we implement it here since some code
 * still uses it
 */
void
clock_delay(unsigned int i)
{
  clock_delay_usec(i);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
