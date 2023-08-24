#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Drop maximum to PM1 to have the UART on all the time */
#define LPM_CONF_MAX_PM     1

/* #define NETSTACK_CONF_WITH_IPV6 0 */
/* #define LINKADDR_CONF_SIZE 2 */

/* Override serial-line defaults */
#define SERIAL_LINE_CONF_BUFSIZE 128
#undef IGNORE_CHAR
#undef END
#define IGNORE_CHAR(c) (c == 0x0d)
#define END 0x0a

#define NETSTACK_CONF_RDC     nullrdc_driver

#undef NETSTACK_NETWORK
#define NETSTACK_NETWORK connectivity_net

#endif /* PROJECT_CONF_H_ */
