#ifndef CRYSTAL_TSM_BITMAP_MAPPING_H
#define CRYSTAL_TSM_BITMAP_MAPPING_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_NODES_DEPLOYED              64

void print_nodes();

void map_nodes();

uint64_t flag_node(const uint64_t bitmap, const uint16_t node_id_to_flag);
uint64_t ack_node(const uint64_t acked_bitmap, const uint16_t node_id_to_ack);

#endif // CRYSTAL_TSM_BITMAP_MAPPING_H
