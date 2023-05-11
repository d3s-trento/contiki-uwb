#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_


/*****************************************************************************/
/* Radio configuration                                                       */
/*****************************************************************************/
#define DW1000_CONF_CHANNEL        4
#define DW1000_CONF_PRF            DWT_PRF_64M
#define DW1000_CONF_PLEN           DWT_PLEN_64
#define DW1000_CONF_PAC            DWT_PAC8
#define DW1000_CONF_SFD_MODE       0
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD
#define DW1000_CONF_PREAMBLE_CODE  17

#define DW1000_CONF_SFD_TIMEOUT    (65 + 32 + 8 - 8)
#define DW1000_CONF_PG_DELAY       0x95

#define DW1000_CONF_SMART_TX_POWER_6M8 0
#define DW1000_CONF_TX_POWER       0x9a9a9a9a

#define STATETIME_CONF_ON 1

// OLD
#define PERIOD (500000*UUS_TO_DWT_TIME_32) //(500000*UUS_TO_DWT_TIME_32)               // ~ 500ms
#define SLOT_DURATION (114416) //(500*UUS_TO_DWT_TIME_32)           // ~ 1 ms

#define TSM_DEFAULT_MINISLOTS_GROUPING 1
#define TIMEOUT (36364) //(SLOT_DURATION - 350*UUS_TO_DWT_TIME_32) // slot timeout

// Upper boundary for random TX jitter
// (if there is no jitter, it is likely that always the same device is heard)
// Make sure to disable or increase the preamble timeout if you use TX jitter
// #define MAX_JITTER (6)
// #define JITTER_STEP (20*UUS_TO_DWT_TIME_32)

#define JITTER_STEP         (0x2) // DWT32 LSB is 4ns, 8ns is therefore 0x2, or 1 << 1
#define MAX_JITTER_MULT     (125) // 1us/8ns
#define SYMBOLS_WAIT        (160)

// CUSTOM 
/** Max number of hops to reach all the nodes starting from the sink*/
//#define CONF_H     7
#define CONF_H     13

/** Number of TSM slots that make up a FS slot */
#define FS_MACROSLOT 3
#define FS_MINISLOT (3)

/** Number of times the bootstrap pkt is sent */
#define CONF_B     2

/** Max number of nodes */
#define MAX_NODES 96

/** Enable debugging of FS (gives access to latency data) */
#define FS_DEBUG 1

#define SINK_ID 19

#define R_NUMBER_SEED  (123)
#define R_NUMBER_EXCL  ((((uint64_t)1) << 61) | (((uint64_t)1) << 62) | (((uint64_t)1) << 63))

#define FS_PER_EPOCH (100)
#define N_ORIGINATORS (60)

#define ENHANCED_BOOTSTRAP (0)
#define SNIFF_FS_OFF_TIME (28)

#define MAX_FS_LATENCY (587 * UUS_TO_DWT_TIME_32)

#if SNIFF_FS_OFF_TIME != 0
#define SNIFF_FS (1)
#else
#define SNIFF_FS (0)
#endif

#define MAX_NODES_DEPLOYED 64
#define NODES_DEPLOYED 1,2,3,4,6,7,8,9,10,12,14,15,16,17,18,24,25,27,28,29,30,31,32,33,35,36,100,102,104,105,106,109,110,111,119,121,122,123,124,125,127,128,129,130,132,133,134,135,136,137,138,139,140,141,143,144,145,148,149,151,153

#endif
