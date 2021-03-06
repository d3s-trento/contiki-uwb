#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#ifndef APP_RADIO_CONF
#define APP_RADIO_CONF 6
#endif
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
/*                          GLOSSY CONFIGURATION                             */
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF:    0 |
 *                                       1
 *
 * If set to 1 the slot is dynamically estimated based on Rx-Tx and Tx-Rx
 * pairs.
 *
 * If set to 0, the slot is set based on the estimated transmission time
 * of the first frame received.
 */
#define GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF   0
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_LOG_LEVEL_CONF:   GLOSSY_LOG_NONE_LEVEL  |
 *                          GLOSSY_LOG_ALL_LEVELS  |
 *                          GLOSSY_LOG_INFO_LEVEL  |
 *                          GLOSSY_LOG_DEBUG_LEVEL |
 *                          GLOSSY_LOG_ERROR_LEVEL
 */
//#define GLOSSY_LOG_LEVEL_CONF GLOSSY_LOG_DEBUG_LEVEL
//#define GLOSSY_LOG_LEVEL_CONF GLOSSY_LOG_ALL_LEVELS
//#define GLOSSY_LOG_LEVEL_CONF GLOSSY_LOG_NONE_LEVEL
#define GLOSSY_LOG_LEVEL_CONF GLOSSY_LOG_ERROR_LEVEL
/*---------------------------------------------------------------------------*/
/*                          CRYSTAL CONFIG                                   */
/*---------------------------------------------------------------------------*/

/*
 * All crystal config should be generated by simgen at test_tools/simgen.py
 */
#define ENERGEST_CONF_ON 0  // Disable energy pritings computed by ENERGEST

#define STATETIME_CONF_ON 1 // enable statetime on the dw1000 radio


/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
