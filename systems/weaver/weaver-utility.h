/*
 * Copyright (c) 2020, University of Trento.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 * \author    Matteo Trobinger    <matteo.trobinger@unitn.it>
 * \author    Davide Vecchia      <davide.vecchia@unitn.it>
 */
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
