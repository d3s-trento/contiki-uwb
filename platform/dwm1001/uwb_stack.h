
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
#else /* DW1000_CONF_DATA_RATE==DWT_BR_110k */
#define NULLRDC_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 1000)
#endif /* DW1000_CONF_DATA_RATE==DWT_BR_110k */

#define NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1000)

#endif /* HW_ACKS==1 */


/* Network stack */
#ifndef NETSTACK_CONF_NETWORK
#if NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_NETWORK sicslowpan_driver
#else
#define NETSTACK_CONF_NETWORK rime_driver
#endif /* NETSTACK_CONF_WITH_IPV6 */
#endif /* NETSTACK_CONF_NETWORK */

/* Network setup */
#if defined(NETSTACK_CONF_RADIO) && NETSTACK_CONF_RADIO != dw1000_driver
#error Excpected NETSTACK_CONF_RADIO is dw1000_driver but is not!!! Something goes wrong!!
#endif

//#define NETSTACK_CONF_RADIO         dw1000_driver

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
