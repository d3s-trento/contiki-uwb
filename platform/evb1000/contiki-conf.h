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

/*
 * \file
 *		EVB1000 Contiki Configuration
 *
 * \author
 * 		Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#ifndef CONTIKI_CONF_H
#define CONTIKI_CONF_H

#include <stdint.h>
/*---------------------------------------------------------------------------*/
/* Include Project Specific conf */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */

#include "platform-conf.h"

/*---------------------------------------------------------------------------*/
/* Compiler configurations */
#define CCIF
#define CLIF
/*---------------------------------------------------------------------------*/
/* Platform-specific definitions */
#define CLOCK_CONF_SECOND 1000

/* Platform Typedefs */
typedef uint32_t clock_time_t;
typedef uint32_t uip_stats_t;

/* Clock (time) comparison macro */
#define CLOCK_LT(a, b)  ((signed long)((a) - (b)) < 0)

/*
 * rtimer.h typedefs rtimer_clock_t as unsigned short. We need to define
 * RTIMER_CLOCK_DIFF to override this
 */
typedef uint32_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a, b)     ((int32_t)((a) - (b)))
/*---------------------------------------------------------------------------*/
#ifndef DW1000_CONF_FRAMEFILTER
#define DW1000_CONF_FRAMEFILTER 1
#endif

#define HW_ACKS 0

#if HW_ACKS==1
#ifndef DW1000_CONF_AUTOACK
#define DW1000_CONF_AUTOACK 1
#endif

#ifndef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK               1
#endif

#ifndef NULLRDC_CONF_802154_AUTOACK_HW
#define NULLRDC_CONF_802154_AUTOACK_HW            1
#endif

#ifndef NULLRDC_CONF_SEND_802154_ACK
#define NULLRDC_CONF_SEND_802154_ACK              0
#endif

#else /* HW_ACKS==1 */

#ifndef DW1000_CONF_AUTOACK
#define DW1000_CONF_AUTOACK 0
#endif

#ifndef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK               1
#endif

#ifndef NULLRDC_CONF_802154_AUTOACK_HW
#define NULLRDC_CONF_802154_AUTOACK_HW            0
#endif

#ifndef NULLRDC_CONF_SEND_802154_ACK
#define NULLRDC_CONF_SEND_802154_ACK              1
#endif

#if DW1000_CONF_DATA_RATE==DWT_BR_110k
#define NULLRDC_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 100)
#else
#define NULLRDC_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 1000)
#endif
#define NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1000)

#endif /* HW_ACKS==1 */


/* Network stack */
#ifndef NETSTACK_CONF_NETWORK
#if NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_NETWORK sicslowpan_driver
#else
#define NETSTACK_CONF_NETWORK rime_driver

// Instruct chameleon to not duplicate MAC addresses in its headers
#define CHAMELEON_CONF_WITH_MAC_LINK_ADDRESSES 1

#endif /* NETSTACK_CONF_WITH_IPV6 */
#endif /* NETSTACK_CONF_NETWORK */

/* Network setup */
#define NETSTACK_CONF_RADIO         dw1000_driver

#ifndef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC           csma_driver
#endif

#ifndef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC           nullrdc_driver
#endif

#ifndef NETSTACK_CONF_FRAMER
#define NETSTACK_CONF_FRAMER        framer_802154
#endif

#ifndef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID             0xABCD
#endif
/*---------------------------------------------------------------------------*/
#if NETSTACK_CONF_WITH_IPV6
/* Addresses, Sizes and Interfaces */
/* 8-byte addresses here, 2 otherwise */
#define LINKADDR_CONF_SIZE                   8
#define UIP_CONF_LL_802154                   1
#define UIP_CONF_LLH_LEN                     0
#define UIP_CONF_NETIF_MAX_ADDRESSES         3

/* TCP, UDP, ICMP */
#ifndef UIP_CONF_TCP
#define UIP_CONF_TCP                         1
#endif
#ifndef UIP_CONF_TCP_MSS
#define UIP_CONF_TCP_MSS                    64
#endif
#define UIP_CONF_UDP                         1
#define UIP_CONF_UDP_CHECKSUMS               1
#define UIP_CONF_ICMP6                       1

/* ND and Routing */
#ifndef UIP_CONF_ROUTER
#define UIP_CONF_ROUTER                      1
#endif

#define UIP_CONF_ND6_SEND_RA                 0
#define UIP_CONF_IP_FORWARD                  0
#define RPL_CONF_STATS                       0

#define UIP_CONF_ND6_REACHABLE_TIME     600000
#define UIP_CONF_ND6_RETRANS_TIMER       10000

#ifndef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS        16
#endif
#ifndef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES                 16
#endif

/* uIP */
#ifndef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE              1300
#endif

#define UIP_CONF_IPV6_QUEUE_PKT              0
#define UIP_CONF_IPV6_CHECKS                 1
#define UIP_CONF_IPV6_REASSEMBLY             0
#define UIP_CONF_MAX_LISTENPORTS             8

/* 6lowpan */
#define SICSLOWPAN_CONF_COMPRESSION          SICSLOWPAN_COMPRESSION_HC06
#ifndef SICSLOWPAN_CONF_COMPRESSION_THRESHOLD
#define SICSLOWPAN_CONF_COMPRESSION_THRESHOLD 63
#endif
#ifndef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG                 1
#endif
#define SICSLOWPAN_CONF_MAXAGE               8

/* Define our IPv6 prefixes/contexts here */
#define SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS    1
#ifndef SICSLOWPAN_CONF_ADDR_CONTEXT_0
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 { \
  addr_contexts[0].prefix[0] = UIP_DS6_DEFAULT_PREFIX_0; \
  addr_contexts[0].prefix[1] = UIP_DS6_DEFAULT_PREFIX_1; \
}
#endif

#endif /* NETSTACK_CONF_WITH_IPV6 */
/*---------------------------------------------------------------------------*/
/* Common network stack configuration */
#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM			8
#endif
/*---------------------------------------------------------------------------*/
#endif /* CONTIKI_CONF_H */
