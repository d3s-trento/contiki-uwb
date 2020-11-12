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
 * \addtogroup nrf52dk nRF52 Development Kit
 * @{
 *
 * \addtogroup nrf52dk-platform-conf Platform configuration
 * @{
 * \file
 *         Platform features configuration.
 * \author
 *         Wojciech Bober <wojciech.bober@nordicsemi.no>
 *         Davide Molteni <davide.molteni@unitn.it>
 *
 */

/* include the platform-conf of module dwm1001 */
#include "dwm1001-platform-conf.h"

/* include board definition */
#include "dwm1001-dev-board.h"

#include "nrf_gpio.h"

#ifndef PLATFORM_CONF_H_
#define PLATFORM_CONF_H_


#define PLATFORM_HAS_BATTERY                    0
#define PLATFORM_HAS_RADIO                      0
#define PLATFORM_HAS_TEMPERATURE                1
#define PLATFORM_HAS_LEDS                       1

/**
 * \brief If set to 1 then LED1 and LED2 are used by the
 *        platform to indicate BLE connection state.
 */
#define PLATFORM_INDICATE_BLE_STATE             1
/** @} */

/**
 * \name Button configurations
 *
 * @{
 */
/* Notify various examples that we have Buttons */
#define PLATFORM_HAS_BUTTON      1

#ifndef REPLY_PLATFORM
/* configre button's macro*/
#define DWM1001_USER_BUTTON   2
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP
#define BUTTONS_ACTIVE_STATE  0
#define DWM1001_BUTTON_MASK   (1 << DWM1001_USER_BUTTON)
#else
#define DWM1001_USER_BUTTON   28
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP
#define BUTTONS_ACTIVE_STATE  1
#define DWM1001_BUTTON_MASK   (1 << DWM1001_USER_BUTTON)
#endif


/** @} */
/*---------------------------------------------------------------------------*/
/** @}
 *  @}
 *  @}
 */
#endif /* PLATFORM_CONF_H_ */
