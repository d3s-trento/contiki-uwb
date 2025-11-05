#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#include "print-def.h"

#ifndef APP_RADIO_CONF
#define APP_RADIO_CONF 7
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

#elif APP_RADIO_CONF == 7

#define DW1000_CONF_CHANNEL        3
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_64
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  9
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

// TODO: Should be moved
#define SINK_RADIUS 13

#define FS_MACROSLOT 3

#define SLOT_DURATION (20*UUS_TO_DWT_TIME_32)
#define SAMU_DEFAULT_SLIVERS_PER_ACTION (24)
#define TIMEOUT (137*UUS_TO_DWT_TIME_32)

#define MAX_LATENCY_FS ((uint32_t)(564 * UUS_TO_DWT_TIME_32))
//#define MAX_LATENCY_FS ((uint32_t)(620 * UUS_TO_DWT_TIME_32))
#define SNIFF_FS_OFF_TIME (26)

#define SAMU_CONF_DEFAULT_RXGUARD (1*UUS_TO_DWT_TIME_32)

#if SNIFF_FS_OFF_TIME != 0
#define SNIFF_FS (1)
#else
#define SNIFF_FS (0)
#endif

#define LOGGING 1

#define SAMU_LOGS_MAX 192
#define FS_DEBUG 0

#define ENHANCED_BOOTSTRAP 0

// // S phase defs
// #define UNSYNCHRONIZED_GUARD_BEFORE (50 * UUS_TO_DWT_TIME_32)
// #define UNSYNCHRONIZED_GUARD_AFTER  (20 * UUS_TO_DWT_TIME_32)
// #define UNSYNCHRONIZED_TIMEOUT      (63 * UUS_TO_DWT_TIME_32) // 12 bytes (7 of SAMU, 1 of type tag, 2 of epoch, 2 of FCS) -> 40us + 3us guard + 20us unsynchronized guard
															  //
// #define UNSYNCHRONIZED_GUARD_BEFORE (16 * UUS_TO_DWT_TIME_32)
// #define UNSYNCHRONIZED_GUARD_AFTER  (16 * UUS_TO_DWT_TIME_32)
// #define UNSYNCHRONIZED_TIMEOUT      (59 * UUS_TO_DWT_TIME_32) // 12 bytes (7 of SAMU, 1 of type tag, 2 of epoch, 2 of FCS) -> 40us + 3us guard + 16us unsynchronized guard

#define UNSYNCHRONIZED_GUARD_BEFORE (12 * UUS_TO_DWT_TIME_32)
#define UNSYNCHRONIZED_GUARD_AFTER  ( 4 * UUS_TO_DWT_TIME_32)
#define UNSYNCHRONIZED_TIMEOUT      (47 * UUS_TO_DWT_TIME_32) // 12 bytes (7 of SAMU, 1 of type tag, 2 of epoch, 2 of FCS) -> 40us + 3us guard +  4us unsynchronized guard

// A phase defs
#define A_PHASE_TIMEOUT (55 * UUS_TO_DWT_TIME_32) // 23 bytes (7 of SAMU, 1 of type tag, 2 of epoch, 1 of flag, 8 of bitmap, 2 of app seqn, 2 of FCS) -> 52us + 3us guard

#define FS_ONLY_SLIVER (45)

#define DYNAMIC_SLOTS 0

#if (CRYSTAL_VARIANT != 0) && (CRYSTAL_VARIANT != 3)
  #error "For this test this configuration is not supported"
#endif

#if (PAYLOAD_LENGTH != 100) && (PAYLOAD_LENGTH != 2)
  #error "For this test this configuration is not supported"
#endif

// #if CRYSTAL_VARIANT == 0
// #pragma message "Crystal variant: No FS"
// #elif CRYSTAL_VARIANT == 3
// #pragma message "Crystal variant: Simple FS"
// #else
// #error "A"
// #endif

// T phase defs
// (14 + x) B 7B of SAMU, 1 of type of tag, 2 of src, 2 of app seqn, x of payload, 2 of fcs
#if PAYLOAD_LENGTH == 100

  #define T_PHASE_TIMEOUT (164 * UUS_TO_DWT_TIME_32)

  #if (DYNAMIC_SLOTS != 0)
    #define S_PHASE_GROUPING (25)
    #define T_PHASE_GROUPING (33)
    #define A_PHASE_GROUPING (22)

    #define POST_S_SLIVER (2)

    #if CRYSTAL_VARIANT == 0
      // If it is a no fs crystal variant
      #define POST_A_SLIVER (5)
      #define POST_FS_SLIVER (0)
    #elif CRYSTAL_VARIANT == 3
      #define POST_A_SLIVER (1)
      #define POST_FS_SLIVER (0)
    #endif
  #else
    #define S_PHASE_GROUPING (33)
    #define T_PHASE_GROUPING (33)
    #define A_PHASE_GROUPING (33)

    #define POST_S_SLIVER  (0)
    #define POST_A_SLIVER  (0)
    #define POST_FS_SLIVER (0)
  #endif

#elif PAYLOAD_LENGTH == 2
  #define T_PHASE_TIMEOUT (63* UUS_TO_DWT_TIME_32)

  #if DYNAMIC_SLOTS != 0
    #define S_PHASE_GROUPING (25)
    // #define T_PHASE_GROUPING (21)
    #define T_PHASE_GROUPING (22)
    #define A_PHASE_GROUPING (22)

    #define POST_S_SLIVER (2)
  #else
    #define S_PHASE_GROUPING (25)
    #define T_PHASE_GROUPING (25)
    #define A_PHASE_GROUPING (25)

    #define POST_S_SLIVER (0)
  #endif

  #define POST_A_SLIVER (0)
  #define POST_FS_SLIVER (0)
#endif

#define EPOCH_INIT_DURATION (5000 * UUS_TO_DWT_TIME_32)
#define SAMU_PRE_EPOCH_PROCEDURE_GUARD_US 7000
#define STATETIME_PRE_EPOCH_DURATION (100 * UUS_TO_DWT_TIME_32)
#define PRE_EPOCH_DURATION ((70 + 150 + 73)* UUS_TO_DWT_TIME_32)

/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
