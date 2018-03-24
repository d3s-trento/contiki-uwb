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

/*---------------------------------------------------------------------------*/
/*
 * Default DW1000 Configuration:
 *
 * Channel: 4
 * PRF: 16M
 * Preamble Length (PLEN): 128
 * Preamble Acquisition Count (PAC): 8
 * SFD Mode: Standard-compliant
 * Bit Rate: 6.8 Mbps
 * Physical Header Mode: Standard-compliant
 * SFD Timeout: 128 + 8 - 8
 * TX/RX Preamble Code: 7
 *
 */

#ifdef DW1000_CONF_COMPENSATE_BIAS
#define DW1000_COMPENSATE_BIAS DW1000_CONF_COMPENSATE_BIAS
#else
#define DW1000_COMPENSATE_BIAS 1
#endif

#ifdef DW1000_CONF_CHANNEL
#define DW1000_CHANNEL DW1000_CONF_CHANNEL
#else
#define DW1000_CHANNEL 4
#endif

#ifdef DW1000_CONF_PRF
#define DW1000_PRF DW1000_CONF_PRF
#else
#define DW1000_PRF DWT_PRF_16M
#endif

#ifdef DW1000_CONF_PLEN
#define DW1000_PLEN DW1000_CONF_PLEN
#else
#define DW1000_PLEN DWT_PLEN_128
#endif

#ifdef DW1000_CONF_PAC
#define DW1000_PAC DW1000_CONF_PAC
#else
#define DW1000_PAC DWT_PAC8
#endif

#ifdef DW1000_CONF_SFD_MODE
#define DW1000_SFD_MODE DW1000_CONF_SFD_MODE
#else
#define DW1000_SFD_MODE 0
#endif

#ifdef DW1000_CONF_DATA_RATE
#define DW1000_DATA_RATE DW1000_CONF_DATA_RATE
#else
#define DW1000_DATA_RATE DWT_BR_6M8
#endif

#ifdef DW1000_CONF_PHR_MODE
#define DW1000_PHR_MODE DW1000_CONF_PHR_MODE
#else
#define DW1000_PHR_MODE DWT_PHRMODE_STD
#endif

/* The following values should be computed/selected depending on other parameters
 * configuration. For instance, the preamble code depends on the channel being
 * used. Moreover, for every channel there are at least 2 preamble codes available.
 */
#ifdef DW1000_CONF_PREAMBLE_CODE
#define DW1000_PREAMBLE_CODE DW1000_CONF_PREAMBLE_CODE
#else
#define DW1000_PREAMBLE_CODE 7
#endif

#ifdef DW1000_CONF_SFD_TIMEOUT
#define DW1000_SFD_TIMEOUT DW1000_CONF_PHR_MODE
#else
#define DW1000_SFD_TIMEOUT (128 + 8 - 8)
#endif

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

int dw1000_configure(dwt_config_t *cfg);

#endif /* DW1000_H */
