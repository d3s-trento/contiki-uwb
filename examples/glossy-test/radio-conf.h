#ifndef RADIO_CONF_H_
#define RADIO_CONF_H_

#define DW1000_CONF_FRAMEFILTER 0

#define APP_RADIO_CONF 6
/*---------------------------------------------------------------------------*/
#if APP_RADIO_CONF == 1
#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_128
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  17
#define DW1000_CONF_SFD_TIMEOUT    (129 + 32 + 8 - 8)
#define DW1000_CONF_PG_DELAY       0x95

#elif APP_RADIO_CONF == 2

#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_16M
#define DW1000_CONF_PLEN           DWT_PLEN_128
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  7
#define DW1000_CONF_SFD_TIMEOUT    (129 + 32 + 8 - 8)
#define DW1000_CONF_PG_DELAY       0x95

#elif APP_RADIO_CONF == 3

#define DW1000_CONF_CHANNEL        2
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_128
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  9
#define DW1000_CONF_SFD_TIMEOUT    (129 + 32 + 8 - 8)
#define DW1000_CONF_PG_DELAY       0xc2

#elif APP_RADIO_CONF == 4

#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_1024
#define DW1000_CONF_PAC            DWT_PAC32
#define DW1000_CONF_SFD_MODE       1
#define DW1000_CONF_DATA_RATE      DWT_BR_110K
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  17
#define DW1000_CONF_SFD_TIMEOUT    (1025 + 32 + 64 - 32)
#define DW1000_CONF_PG_DELAY       0x95

#elif APP_RADIO_CONF == 5

#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_2048
#define DW1000_CONF_PAC            DWT_PAC64
#define DW1000_CONF_SFD_MODE       1
#define DW1000_CONF_DATA_RATE      DWT_BR_110K
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  17
#define DW1000_CONF_SFD_TIMEOUT    (2049 + 32 + 64 - 64) // TODO: check this out!
#define DW1000_CONF_PG_DELAY       0x95

#elif APP_RADIO_CONF == 6

#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_64
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  17
#define DW1000_CONF_SFD_TIMEOUT    (65 + 32 + 8 - 8) // TODO: check this out!
#define DW1000_CONF_PG_DELAY       0x95

#endif
/*---------------------------------------------------------------------------*/
#endif /* RADIO_CONF_H_ */
