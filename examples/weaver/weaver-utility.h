#ifndef WEAVER_UTILITY_H_
#define WEAVER_UTILITY_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_NODES_DEPLOYED              64

void map_nodes();

/** Retrieve node ids that are flagged as 1 in the given bitmap.
 * Stop when reaching len nodes (no more space available in the destination
 * array).
 * Return the number of nodes unmapped.
 */
size_t unmap_nodes(const uint64_t *map, uint16_t *dest_array, const size_t len);

uint64_t flag_node(const uint64_t bitmap, const uint16_t node_id_to_flag);
uint64_t ack_node(const uint64_t acked_bitmap, const uint16_t node_id_to_ack);
bool is_node_acked(const uint64_t acked_bitmap, const uint16_t node_id_to_search);
void print_bitmap(char *prefix, const size_t prefix_len, const uint64_t bitmap);
void print_acked(const uint64_t acked_bitmap);

#endif // WEAVER_UTILITY_H_
