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

/**
 * \addtogroup platform
 * @{
 *
 * \addtogroup dwm1001 
 * @{
 *
 * \addtogroup dwm1001-module Development
 * @{
 * \file
 *         Platform features configuration.
 * \author
 *         Wojciech Bober <wojciech.bober@nordicsemi.no>
 *         Davide Molteni <davide.molteni@unitn.it>
 *
 */

#ifndef DWM1001_PLATFORM_CONF_H_
#define  DWM1001_PLATFORM_CONF_H_

/**
 * \brief nRF52 RTC instance to be used for Contiki clock driver.
 * \note RTC 0 is used by the SoftDevice.
 */
#define PLATFORM_CLOCK_RTC_INSTANCE_ID     1


/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Compiler configuration and platform-specific type definitions
 *
 * Those values are not meant to be modified by the user
 * @{
 */

/* If you need to change the clock frequency, make sure to
 * adjust the NRF SDK setting NRFX_RTC_DEFAULT_CONFIG_FREQUENCY
 */
#define CLOCK_CONF_SECOND 1024

/* Compiler configurations */
#define CCIF
#define CLIF

/* Platform typedefs */
typedef uint32_t clock_time_t;
typedef uint32_t uip_stats_t;

/* Clock (time) comparison macro */
#define CLOCK_LT(a, b)  ((signed long)((a) - (b)) < 0)


/* RTimer frequency.
 * It depends on the selected clock source for RTimer.
 *
 */
#if NRF52_RTIMER_USE_HFCLK

/* Setting for HFCLK.
 * Note that this setting should correspond with the
 * frequency in NRF configuration (NRFX_TIMER_DEFAULT_CONFIG_FREQUENCY).
 *
 * Related files: 
 *    - config/nrfx_config.h 
 *    - config/sdk_config.h
 */
#define RTIMER_ARCH_SECOND 1000000L

/**
 * \brief nRF52 timer instance to be used for Contiki rtimer driver.
 * \note Timer 0 is used by the SoftDevice.
 */
#define PLATFORM_RTIMER_TIMER_INSTANCE_ID   1


#else

/* Setting for LFCLK */
#define RTIMER_ARCH_SECOND 32768L

/**
 * \brief nRF52 rtc instance to be used for Contiki rtimer driver.
 * \note RTC 0 is used by the SoftDevice.
 */
#define PLATFORM_RTIMER_RTC_INSTANCE_ID   2
#endif

/*
 * rtimer.h typedefs rtimer_clock_t as unsigned short. We need to define
 * RTIMER_CLOCK_DIFF to override this
 */
typedef uint32_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a,b)     ((int32_t)((a)-(b)))



#ifndef DW1000_CONF_RX_ANT_DLY
#define DW1000_CONF_RX_ANT_DLY 16455 // TODO: needs calibration
#endif

#ifndef DW1000_CONF_TX_ANT_DLY
#define DW1000_CONF_TX_ANT_DLY 16455 // TODO: needs calibration
#endif

/** @} */


#endif /* DWM1001_PLATFORM_CONF_H_ */
