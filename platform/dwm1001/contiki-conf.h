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
 * \addtogroup nrf52dk
 * @{
 *
 * \addtogroup nrf52dk-contikic-conf Contiki configuration
 * @{
 *
 * \file
 *  Contiki configuration for the nRF52 DK
 */
#ifndef CONTIKI_CONF_H
#define CONTIKI_CONF_H

#include <stdint.h>
/*---------------------------------------------------------------------------*/
/* Include Project Specific conf */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */
/*---------------------------------------------------------------------------*/
/* Include platform peripherals configuration */
#include "platform-conf.h"
/*---------------------------------------------------------------------------*/
#ifndef DW1000_CONF_FRAMEFILTER
#define DW1000_CONF_FRAMEFILTER 1
#endif


/*---------------------------------------------------------------------------
inlcude file to have last_reset_reason variable
*/
#include "reset_reason.h"

/*---------------------------------------------------------------------------
 * RADIO STACK NOTE:
 * here after some test are done to choose right radio stack....
 * radio is defined with the macro
 */

/* #define XSTR(x) STR(x) */
/* #define STR(x) #x */

//if is not set deafult radio is dw1000
#ifndef NETSTACK_CONF_RADIO
#define NETSTACK_CONF_RADIO dw1000_driver
#endif

#define HW_ACKS 0
/*---------------------------------------------------------------------------*/
//#if NETSTACK_CONF_RADIO == dw1000_driver
#include "uwb_stack.h"
//#endif
/*---------------------------------------------------------------------------*/
/**
 * \name Generic Configuration directives
 *
 * @{
 */
#ifndef ENERGEST_CONF_ON
#define ENERGEST_CONF_ON                     1 /**< Energest Module */
#endif
/** @} */
#endif /* CONTIKI_CONF_H */
/**
 * @}
 * @}
 */
