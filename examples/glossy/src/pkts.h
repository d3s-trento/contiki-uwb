#ifndef PKTS_H
#define PKTS_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
  /* TSM_HDR: uint16_t tx_delay; // 2B
   *          uint32_t slot_idx; // 4B
   *          uint8_t crc;       // 1B
   */
  uint16_t epoch;                // 2B
  uint16_t node_id;              // 2B // TODO: Change to originator_id
  uint8_t  flags;                // 1B
  uint8_t  hop_distance;         // 1B
} pkt_t;


typedef enum {
  PKT_TYPE_SYNCH = 1 << 0,
  PKT_TYPE_EVENT = 1 << 1,
} pkt_flag_t; // NOTE: Maybe do some optimization to consider a completely empty bitmask one of these but it seems having very little impact

#endif // PKTS_H
