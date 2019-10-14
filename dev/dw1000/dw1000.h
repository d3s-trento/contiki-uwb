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
 *      Contiki DW1000 Driver Header File
 *
 * \author
 *      Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#ifndef DW1000_H
#define DW1000_H
/*---------------------------------------------------------------------------*/
#include "dev/radio.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "stdint.h"
#include "stdbool.h"
#include "core/net/linkaddr.h"
#include "contiki-conf.h"


/*---------------------------------------------------------------------------*/
#define DW1000_CRC_LEN 2
/*---------------------------------------------------------------------------*/
/* Maximum Packet Length */
#ifdef DW1000_IEEE802154_EXTENDED
#define DW1000_MAX_PACKET_LEN 265
#else
#define DW1000_MAX_PACKET_LEN 127
#endif
/*---------------------------------------------------------------------------*/
/* Hardware Automatic ACK configuration */
#ifdef DW1000_CONF_AUTOACK
#define DW1000_AUTOACK DW1000_CONF_AUTOACK
#else /* DW1000_CONF_AUTOACK */
#define DW1000_AUTOACK 0
#endif /* DW1000_CONF_AUTOACK */

/* Hardware Automatic ACK configuration */
#ifdef DW1000_CONF_FRAMEFILTER
#define DW1000_FRAMEFILTER DW1000_CONF_FRAMEFILTER
#else /* DW1000_CONF_FRAMEFILTER */
#define DW1000_FRAMEFILTER 1
#endif /* DW1000_CONF_FRAMEFILTER */

#if DW1000_AUTOACK && !DW1000_FRAMEFILTER
#error "Auto-ACK is only possible if frame filtering is enabled"
#endif

#ifdef DW1000_CONF_AUTOACK_DELAY
#define DW1000_AUTOACK_DELAY DW1000_CONF_AUTOACK_DELAY
#else /* DW1000_CONF_AUTOACK_DELAY */
/* #define DW1000_AUTOACK_DELAY 3 // Min supported by the radio */
/* #define DW1000_AUTOACK_DELAY 12 // Min mandated by IEEE 802.15.4 for HRP UWB PHY */
#define DW1000_AUTOACK_DELAY 255 /* Max supported by the radio */
#endif /* DW1000_CONF_AUTOACK_DELAY */

#ifdef DW1000_CONF_RANGING_ENABLED
#define DW1000_RANGING_ENABLED DW1000_CONF_RANGING_ENABLED
#else
#define DW1000_RANGING_ENABLED 1
#endif

/*---------------------------------------------------------------------------*/
extern const struct radio_driver dw1000_driver;
/*---------------------------------------------------------------------------*/

typedef enum {DW1000_RNG_SS, DW1000_RNG_DS} dw1000_rng_type_t;

bool range_with(linkaddr_t *dst, dw1000_rng_type_t type);

#endif /* DW1000_H */
