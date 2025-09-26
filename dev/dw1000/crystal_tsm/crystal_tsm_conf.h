#ifndef CRYSTAL_CONF_H_
#define CRYSTAL_CONF_H_

#ifdef CRYSTAL_CONF_PERIOD_MS
#define CRYSTAL_CONF_PERIOD ((uint32_t)CRYSTAL_CONF_PERIOD_MS*1000*UUS_TO_DWT_TIME_32)
#endif

#ifndef CRYSTAL_CONF_PERIOD
#define CRYSTAL_CONF_PERIOD ((uint32_t)1000*1000*UUS_TO_DWT_TIME_32)
#endif

#ifndef CRYSTAL_CONF_IS_SINK
#define CRYSTAL_CONF_IS_SINK 0
#endif

// // TODO: Is this used?
// #ifdef CRYSTAL_CONF_DEF_CHANNEL
// #define CRYSTAL_DEF_CHANNEL CRYSTAL_CONF_DEF_CHANNEL
// #else
// #define CRYSTAL_DEF_CHANNEL 26
// #endif

#ifndef CRYSTAL_CONF_NTX_S
#define CRYSTAL_CONF_NTX_S 3
#endif

// #ifdef CRYSTAL_CONF_DUR_S_MS
// #define CRYSTAL_CONF_DUR_S ((uint32_t) ((double) RTIMER_SECOND*CRYSTAL_CONF_DUR_S_MS/1000) )
// #endif
// 
// #ifndef CRYSTAL_CONF_DUR_S
// #define CRYSTAL_CONF_DUR_S ((uint32_t)RTIMER_SECOND*10/1000)
// #endif

#ifndef CRYSTAL_CONF_NTX_T
#define CRYSTAL_CONF_NTX_T 3
#endif

// #ifdef CRYSTAL_CONF_DUR_T_MS
// #define CRYSTAL_CONF_DUR_T ((uint32_t) ((double) RTIMER_SECOND*CRYSTAL_CONF_DUR_T_MS/1000) )
// #endif
// 
// #ifndef CRYSTAL_CONF_DUR_T
// #define CRYSTAL_CONF_DUR_T ((uint32_t)RTIMER_SECOND*8/1000)
// #endif

#ifndef CRYSTAL_CONF_NTX_A
#define CRYSTAL_CONF_NTX_A 3
#endif

// #ifdef CRYSTAL_CONF_DUR_A_MS
// #define CRYSTAL_CONF_DUR_A ((uint32_t) ((double) RTIMER_SECOND*CRYSTAL_CONF_DUR_A_MS/1000) )
// #endif
// 
// #ifndef CRYSTAL_CONF_DUR_A
// #define CRYSTAL_CONF_DUR_A ((uint32_t)RTIMER_SECOND*8/1000)
// #endif

// the number of empty Ts in a row for the sink to send the sleep command 
#ifndef CRYSTAL_CONF_SINK_MAX_EMPTY_TS 
#define CRYSTAL_CONF_SINK_MAX_EMPTY_TS 2
#endif

// the number of empty TA pairs in a row causing a node to sleep
#ifndef CRYSTAL_CONF_MAX_SILENT_TAS
#define CRYSTAL_CONF_MAX_SILENT_TAS 2
#endif

// the number of missing acks in a row causing a node to sleep
#ifndef CRYSTAL_CONF_MAX_MISSING_ACKS
#define CRYSTAL_CONF_MAX_MISSING_ACKS 4
#endif

// the number of noisy empty Ts in a row for the sink to send the sleep command
// zero means that the noise-sampling feature during T slots is disabled
//#ifndef CRYSTAL_CONF_SINK_MAX_NOISY_TS
//#define CRYSTAL_CONF_SINK_MAX_NOISY_TS 6
//#endif

// the number of noisy empty As for a node to sleep
// zero means that the noise-sampling feature during A slots is disabled
//#ifndef CRYSTAL_CONF_MAX_NOISY_AS
//#define CRYSTAL_CONF_MAX_NOISY_AS CRYSTAL_CONF_SINK_MAX_NOISY_TS
//#endif

//#ifndef CRYSTAL_CONF_CCA_THRESHOLD
//#define CRYSTAL_CONF_CCA_THRESHOLD -32
//#endif

//#define CRYSTAL_CCA_THRESHOLD CRYSTAL_CONF_CCA_THRESHOLD

//#ifdef CRYSTAL_CONF_CCA_COUNTER_THRESHOLD
//#define CRYSTAL_CCA_COUNTER_THRESHOLD CRYSTAL_CONF_CCA_COUNTER_THRESHOLD
//#else
//#define CRYSTAL_CCA_COUNTER_THRESHOLD 80
//#endif

#ifdef CRYSTAL_CONF_SYNC_ACKS
#define CRYSTAL_SYNC_ACKS CRYSTAL_CONF_SYNC_ACKS
#else
#define CRYSTAL_SYNC_ACKS 1
#endif

#define CRYSTAL_LOGS_NONE 0
#define CRYSTAL_LOGS_EPOCH_STATS 1
#define CRYSTAL_LOGS_ALL 2

#ifdef CRYSTAL_CONF_LOGLEVEL
#define CRYSTAL_LOGLEVEL CRYSTAL_CONF_LOGLEVEL
#else
#define CRYSTAL_LOGLEVEL CRYSTAL_LOGS_NONE
#endif

#ifdef CRYSTAL_CONF_DYNAMIC_NEMPTY
#define CRYSTAL_DYNAMIC_NEMPTY CRYSTAL_CONF_DYNAMIC_NEMPTY
#else
#define CRYSTAL_DYNAMIC_NEMPTY 0
#endif

#ifdef CRYSTAL_CONF_N_FULL_EPOCHS
#define CRYSTAL_N_FULL_EPOCHS CRYSTAL_CONF_N_FULL_EPOCHS
#else
#define CRYSTAL_N_FULL_EPOCHS 5
#endif

#ifdef CRYSTAL_CONF_CHHOP_MAPPING
#define CRYSTAL_CHHOP_MAPPING CRYSTAL_CONF_CHHOP_MAPPING
#else
#define CRYSTAL_CHHOP_MAPPING CHMAP_nohop
#endif

#ifdef CRYSTAL_CONF_BSTRAP_CHHOPPING
#define CRYSTAL_BSTRAP_CHHOPPING CRYSTAL_CONF_BSTRAP_CHHOPPING
#else
#define CRYSTAL_BSTRAP_CHHOPPING BSTRAP_nohop
#endif

/* The maximum Crystal packet size */
#ifdef CRYSTAL_CONF_PKTBUF_LEN
#define CRYSTAL_PKTBUF_LEN CRYSTAL_CONF_PKTBUF_LEN
#else
#define CRYSTAL_PKTBUF_LEN 127
#endif

/* The time reserved for the application at the end of Crystal epoch */
#ifdef CRYSTAL_CONF_TIME_FOR_APP
#define CRYSTAL_TIME_FOR_APP CRYSTAL_CONF_TIME_FOR_APP
#else
#define CRYSTAL_TIME_FOR_APP 0
#endif

/* Crystal will notify the application this time before an epoch starts */
#ifdef CRYSTAL_CONF_APP_PRE_EPOCH_CB_TIME
#define CRYSTAL_APP_PRE_EPOCH_CB_TIME CRYSTAL_CONF_APP_PRE_EPOCH_CB_TIME
#else
#define CRYSTAL_APP_PRE_EPOCH_CB_TIME 328 // ~ 10 ms 
#endif

// the number of Ts with reception errors in a row for the sink to send the sleep command
// zero means that the reception error detection feature during T slots is disabled
#ifndef CRYSTAL_CONF_SINK_MAX_RCP_ERRORS_TS
#define CRYSTAL_CONF_SINK_MAX_RCP_ERRORS_TS 6
#endif

#ifndef CRYSTAL_CONF_MAX_RCP_ERRORS_AS
#define CRYSTAL_CONF_MAX_RCP_ERRORS_AS CRYSTAL_CONF_SINK_MAX_RCP_ERRORS_TS
#endif

#define N_SILENT_EPOCHS_TO_STOP_SENDING 3

#endif //CRYSTAL_CONF_H
