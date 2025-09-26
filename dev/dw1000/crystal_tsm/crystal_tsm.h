#ifndef CRYSTAL_TSM_H
#define CRYSTAL_TSM_H

#include PROJECT_CONF_H

#define CRYSTAL_MAX_SCAN_EPOCHS 200
#define CRYSTAL_MAX_PERIOD 5 * 1000000 * UUS_TO_DWT_TIME_32

#include <stdbool.h>
#include <stdint.h>

#include "print-def.h"

#include "crystal_tsm_conf.h"

#define CRYSTAL_VARIANT_NO_FS  (0)
#define CRYSTAL_VARIANT_SIMPLE (3)

/*
 * Expected interaction with the application
 *
 *  Crystal layer                                              App layer
 *       |                                                          |
 *     start                                                        |
 *       |         void app_crystal_start_done(bool success)        |
 *       | -------------------------------------------------------> |
 *       |                                                          |
 *   new epoch                                                      |
 *       |                                                          |
 *       |                   void app_pre_epoch()                   |
 *       | -------------------------------------------------------> |
 *       |                                                          |
 *       |                   uint8_t* app_pre_S()                   |
 *       | -------------------------------------------------------> |
 *       | <------------------------------------------------------- |
 *       |                pointer to payload for sync               |
 *       |                                                          |
 *    S phase                                                       |
 *       |                                                          |
 *       |     void app_post_S(int received, uint8_t* payload)      |
 *       | -------------------------------------------------------> |
 *       |                                                          |
 *       |                    uint8_t* app_pre_T()                  |
 *       | -------------------------------------------------------> |
 *       | <------------------------------------------------------- |
 *       |                pointer to payload for T phase            |
 *       |                                                          |
 *    T phase                                                       |
 *       |                                                          |
 *       |  uint8_t* app_between_TA(int received, uint8_t* payload) |
 *       | -------------------------------------------------------> |
 *       | <------------------------------------------------------- |
 *       |               pointer to payload for A phase             |
 *       |                                                          |
 *    A phase                                                       |
 *       |                                                          |
 *       |     void app_post_A(int received, uint8_t* payload)      |
 *       | -------------------------------------------------------> |
 *       |                                                          |
 *       |                    void app_epoch_end()                  |
 *       | -------------------------------------------------------> |
 *       |                                                          |
 *  Crystal layer                                              App layer
 */


typedef uint16_t crystal_addr_t;
typedef uint16_t crystal_epoch_t;

typedef struct {
  crystal_epoch_t epoch;
  // uint16_t n_ta;
  // uint16_t n_missed_s;
  // uint8_t hops;
} crystal_info_t;

typedef struct {
  uint16_t send_seqn;
  uint16_t recv_seqn;
  uint16_t recv_src;
  uint8_t  acked;
} crystal_app_log_t;

extern crystal_info_t crystal_info;
extern crystal_app_log_t crystal_app_log;

typedef struct {
  // General configuration
  uint32_t period;   // Crystal period in rtimer clock ticks
  uint8_t is_sink;   // Is this node the sink

  // S configuration
  uint8_t ntx_S;     // Number of Glossy TX in S slots
  uint8_t plds_S;    // App. payload size for S slots

  // T configuration
  uint8_t ntx_T;     // Number of Glossy TX in T slots
  uint8_t plds_T;    // App. payload size for T slots

  // A configuration
  uint8_t ntx_A;     // Number of Glossy TX in A slots
  uint8_t plds_A;    // App. payload size for A slots

  // Termination configuration
  uint8_t r;         // Number of empty T slots triggering epoch termination at the sink
  uint8_t y;         // Number of empty TA pairs triggering epoch termination at a non-transmitting node
  uint8_t z;         // Number of empty A slots triggering epoch termination at a transmitting node
  uint8_t x;         // Max. number of TA pairs added when high noise is detected at the sink
  uint8_t xa;        // Max. number of TA pairs added when high noise is detected at a non-sink node

  uint8_t scan_duration; // Scan duration in number of epochs (TBD)
} crystal_config_t;

#define PRINT_CRYSTAL_CONFIG(conf) do {\
 printf("Crystal config. Node ID %hu\n", node_id);\
 printf("Period: %lu\n", (conf).period);\
 printf("Sink: %u\n", (conf).is_sink);\
 printf("S: %u %u\n", (conf).ntx_S, (conf).plds_S);\
 printf("T: %u %u\n", (conf).ntx_T, (conf).plds_T);\
 printf("A: %u %u\n", (conf).ntx_A, (conf).plds_A);\
 printf("Term: %u %u %u %u %u\n", (conf).r, (conf).y, (conf).z, (conf).x, (conf).xa);\
 printf("Scan: %u\n", (conf).scan_duration);\
} while (0)

typedef struct {
  // General data
  crystal_epoch_t epoch; // Used to know how many epochs are remaining

  // Epoch-specific data
#if CRYSTAL_SYNC_ACKS
  uint16_t synced_with_ack;
#endif

#if CRYSTAL_VARIANT != CRYSTAL_VARIANT_NO_FS
  bool last_fs;
#endif
  uint8_t last_ack_flags;

  uint64_t received_bitmap;
  uint64_t ack_bitmap;

  // Run-wide data

  // Peer only
#if CRYSTAL_SYNC_ACKS
  uint16_t n_noack_epochs;
#endif
  uint8_t cumulative_failed_synchronizations;
} crystal_context_t ;

typedef struct {
  crystal_epoch_t epoch;
} __attribute__((packed, aligned(1)))
crystal_sync_hdr_t ;


typedef struct {
  uint16_t src;
} __attribute__((packed, aligned(1)))
crystal_data_hdr_t;

typedef struct {
  crystal_epoch_t epoch;

  uint8_t flags;
  uint64_t ack_bitmap;
} __attribute__((packed, aligned(1)))
crystal_ack_hdr_t;

#define CRYSTAL_TYPE_SYNC 0x01
#define CRYSTAL_TYPE_DATA 0x02
#define CRYSTAL_TYPE_ACK  0x03

#define CRYSTAL_NACK_MASK 1
#define CRYSTAL_ACK_MASK 2

crystal_config_t crystal_get_config();

void crystal_init();
bool crystal_start(const crystal_config_t* conf);

/* == Crystal application interface (callbacks) ==============================*/

/* An interrupt-context callback called by Crystal when it starts or joins a net
 */
void app_crystal_start_done(bool success);

/* An interrupt-context callback called by Crystal before each S slot.
 *
 * returned value: pointer to the application payload for S slot */
uint8_t* app_pre_S();

/* An interrupt-context callback called by crystal at the Flick slot to know if the node will be an originator in this epoch
 *
 * returned value: true if the node will be an originator, false otherwise */
bool app_is_originator();

/* An interrupt-context callback called by crystal at the Flick slot to know if the node will be an originator in this slot (different from epoch one as we could have already ACKed)
 *
 * returned value: true if the node will be an originator, false otherwise */
bool app_has_packet();

/* An interrupt-context callback called by Crystal after S slot.
 * - received: whether a correct packet was received in the slot
 * - payload: pointer to the application payload in T slot packets
 */
void app_post_S(int received, uint8_t* payload);

/* An interrupt-context callback called by Crystal before each T slot.
 *
 * returned value: pointer to the application payload for T slot */
uint8_t* app_pre_T();

/* An interrupt-context callback called by Crystal after each T slot.
 * - received: whether a correct packet was received in the slot
 * - payload: pointer to the application payload in T slot packets
 *
 * returned value: pointer to the application payload for A slot */
uint8_t* app_between_TA(int received, uint8_t* payload, const crystal_data_hdr_t * const data_hdr);

/* An interrupt-context callback called by Crystal after each A slot.
 * - received: whether a correct packet was received in the slot
 * - payload: pointer to the application payload in A slot packets */
void app_post_A(int received, uint8_t* payload, const crystal_ack_hdr_t * const ack_hdr);

/* An interrupt-context callback that signals the end of the active
 * part of the epoch */
void app_epoch_end();

/* An interrupt-context callback that pings the app
 * CRYSTAL_CONF_APP_PRE_EPOCH_CB_TIME before a new epoch starts */
void app_pre_epoch();

#ifndef SINK_RADIUS
#error "The maximum expected distance from the sink (SINK_RADIUS) must be set"
#endif

#endif
