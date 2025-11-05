#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define DW1000_CONF_CHANNEL        3
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_64
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD

#if (DW1000_CONF_CHANNEL == 4) || (DW1000_CONF_CHANNEL == 7)
#define DW1000_CONF_PREAMBLE_CODE  17
#else 
#define DW1000_CONF_PREAMBLE_CODE  9
#endif

#define DW1000_CONF_SFD_TIMEOUT    (65 + 32 + 8 - 8)
#define DW1000_CONF_PG_DELAY       0x95

#define DW1000_CONF_SMART_TX_POWER_6M8 0
#define DW1000_CONF_TX_POWER       0x9a9a9a9a

#define STATETIME_CONF_ON 1

#define FS_MACROSLOT 3

#define MAX_LATENCY_FS ((uint32_t)(564 * UUS_TO_DWT_TIME_32))
#define SNIFF_FS_OFF_TIME (26)

#if SNIFF_FS_OFF_TIME != 0
#define SNIFF_FS (1)
#else
#define SNIFF_FS (0)
#endif


#define SAMU_CONF_DEFAULT_RXGUARD (1*UUS_TO_DWT_TIME_32)

#define SAMU_LOGS_MAX 100

#define GLOBAL_ACK_PERIOD 6

#define WEAVER_LOG_VERBOSE 0

#define WEAVER_BOOT_REDUNDANCY 2

#define RESTART_LOGS 1

// #define UNSYNCHRONIZED_GUARD_BEFORE (50 * UUS_TO_DWT_TIME_32)
// #define UNSYNCHRONIZED_GUARD_AFTER  (20 * UUS_TO_DWT_TIME_32)

#define UNSYNCHRONIZED_GUARD_BEFORE (12 * UUS_TO_DWT_TIME_32)
#define UNSYNCHRONIZED_GUARD_AFTER  ( 4 * UUS_TO_DWT_TIME_32)

#define SLOT_DURATION (20 * UUS_TO_DWT_TIME_32)

// Packet size is:
// x + 26 bytes 
//		7 of SAMU,
//		2 of originator_id,
//		2 of last_heard_originator_id,
//		1 of hop_counter,
//		8 of sink_acked,
//		2 of epoch,
//		2 of seqno,
//		x of extra_payload,
//		2 of FCS
//

#if EXTRA_PAYLOAD_LEN == 2
#define SAMU_DEFAULT_SLIVERS_PER_ACTION 29
#define TIMEOUT (129 * UUS_TO_DWT_TIME_32) // 28B -> 126us + 3us guard
#define UNSYNCHRONIZED_TIMEOUT (149 * UUS_TO_DWT_TIME_32)  // TIMEOUT + 20us unsynchronized guard
#define FS_SLIVER (45)
    
#else
#define SAMU_DEFAULT_SLIVERS_PER_ACTION 40
#define FS_SLIVER (46)
#define TIMEOUT (179 * UUS_TO_DWT_TIME_32) // 126B -> 176us + 3us
#define UNSYNCHRONIZED_TIMEOUT      (183 * UUS_TO_DWT_TIME_32) // TIMEOUT + 4us unsynchronized guard
#endif

#define EPOCH_INIT_DURATION (5000)
#define SAMU_PRE_EPOCH_PROCEDURE_GUARD_US 7000
#define STATETIME_PRE_EPOCH_DURATION (100 * UUS_TO_DWT_TIME_32)
#define PRE_EPOCH_DURATION ((70 + 150 + 73)* UUS_TO_DWT_TIME_32)

#define PERIOD_SINK 5000000

#define GACK_SYNC 0

#endif /* PROJECT_CONF_H_ */
