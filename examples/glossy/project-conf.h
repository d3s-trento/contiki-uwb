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
#define PERIOD (2000000*UUS_TO_DWT_TIME_32)               // ~ 500ms
#define SLOT_DURATION (460*UUS_TO_DWT_TIME_32) //(500*UUS_TO_DWT_TIME_32)           // ~ 1 ms
#define TIMEOUT (150*UUS_TO_DWT_TIME_32) //(SLOT_DURATION - 350*UUS_TO_DWT_TIME_32) // slot timeout

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
#define CONF_H      13
#define SINK_RADIUS 13

/** Number of times the bootstrap pkt is sent */
#define CONF_B     3

#define SINK_ID 19

#define R_NUMBER_SEED  (123)
#define R_NUMBER_EXCL  (~((uint64_t)137432662014))

#define FS_PER_EPOCH (100)
#define N_ORIGINATORS (1)

#define ENHANCED_BOOTSTRAP (0)
#define SNIFF_FS_OFF_TIME (0)

#define MAX_FS_LATENCY (600 * UUS_TO_DWT_TIME_32)

#if SNIFF_FS_OFF_TIME != 0
#define SNIFF_FS (1)
#else
#define SNIFF_FS (0)
#endif

#define TSM_CONF_DEFAULT_RXGUARD (1*UUS_TO_DWT_TIME_32)

#define N_TX 2
#define GLOSSY_REPETITIONS 50
#define GLOSSY_LATENCY_LOG 1

#define TSM_DEFAULT_MINISLOTS_GROUPING 1

#define TSM_LOGS_MAX 100
#define FS_DEBUG 0

#define GLOSSY_MAX_JITTER_MULT (0)
#define GLOSSY_JITTER_STEP     (0)

#define MAX_NODES_DEPLOYED 64
#define NODES_DEPLOYED 1,2,3,4,6,7,8,9,10,12,14,15,16,17,18,24,25,27,28,29,30,31,32,33,35,36,100,102,104,105,106,109,110,111,119,121,122,123,124,125,127,128,129,130,132,133,134,135,136,137,138,139,140,141,143,144,145,148,149,151,153

#endif
