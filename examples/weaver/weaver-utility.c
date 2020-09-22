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
 * \author    Matteo Trobinger    <matteo.trobinger@unitn.it>
 * \author    Davide Vecchia      <davide.vecchia@unitn.it>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "weaver-utility.h"

#include "logging.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...)     printf(__VA_ARGS__)
#else
#define PRINTF(...)     do {} while(0)
#endif /* DEBUG */

extern uint16_t nodes_deployed[];
extern size_t  n_nodes_deployed;

void
print_bitmap(char *prefix, const size_t prefix_len, const uint64_t bitmap)
{
    size_t i = 0;
    if (prefix_len > 0) {
        prefix[prefix_len - 1] = '\0';
        PRINTF("%s %u, ", prefix, logging_context);
    }
    for (; i < n_nodes_deployed; i++) {
        if ((bitmap & ((uint64_t) 1 << i)) > 0) {
            PRINTF("%"PRIu16" ", nodes_deployed[i]);
        }
    }
    PRINTF("\n");
}
/*---------------------------------------------------------------------------*/
void print_acked(const uint64_t acked_bitmap) {
    char prefix[4] = "ACK"; 
    print_bitmap(prefix, 4, acked_bitmap);
}
/*---------------------------------------------------------------------------*/
uint64_t
flag_node(const uint64_t bitmap, const uint16_t node_id_to_flag)
{
    size_t i = 0;
    for (; i < n_nodes_deployed; i++) {
        if (nodes_deployed[i] == node_id_to_flag) {
            return bitmap | ((uint64_t) 1 << i);
        }
    }
    return bitmap;
}
/*---------------------------------------------------------------------------*/
uint64_t
ack_node(const uint64_t acked_bitmap, const uint16_t node_id_to_ack)
{
    return flag_node(acked_bitmap, node_id_to_ack);
}
/*---------------------------------------------------------------------------*/
bool
is_node_acked(const uint64_t acked_bitmap, const uint16_t node_id_to_search)
{
    size_t i = 0;
    for (; i < n_nodes_deployed; i++) {
        if (nodes_deployed[i] == node_id_to_search) {
            return (acked_bitmap & ((uint64_t) 1 << i)) > 0;
        }
    }
    return false;
}
/*---------------------------------------------------------------------------*/
void
map_nodes()
{
    size_t i = 0;
    while (i < MAX_NODES_DEPLOYED && nodes_deployed[i] != 0) {
        i++;
    }
    n_nodes_deployed = i;
}
/*---------------------------------------------------------------------------*/
size_t
unmap_nodes(const uint64_t *map, uint16_t *dest_array, const size_t len)
{
    size_t array_idx = 0;
    size_t i = 0;
    for (; i < n_nodes_deployed && array_idx < len; i++) {
        if ( (((uint64_t) 1 << i) & *map) > 0 ) {
            dest_array[array_idx] = nodes_deployed[i];
            array_idx++;
        }
    }
    return array_idx;
}

