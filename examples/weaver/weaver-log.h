#ifndef WEAVER_LOG_H_
#define WEAVER_LOG_H_

#include <string.h>
#include <stddef.h>
#include <inttypes.h>

#include "trex.h"

typedef struct weaver_log {
    int16_t idx;
    int8_t slot_status;
    uint8_t  node_dist;
    uint16_t originator_id;     // sender id of the (received/transmitted) packet
    uint16_t lhs;               // last heard sender id
    uint64_t acked;             // bitmap of acked nodes
    uint64_t buffer;            // bitmap of nodes whose pkt is in the buffer
} weaver_log_t;

void weaver_log_append(weaver_log_t *entry);
void weaver_log_print();
void weaver_log_init();

#endif  // WEAVER_LOG_H_
