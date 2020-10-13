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
 */
#ifndef RR_TABLE_H
#define RR_TABLE_H

#include "contiki.h"
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#define RR_TABLE_MAX_COUNTER_DEFAULT 5
#define RR_TABLE_MAX_DATA_LEN 127

typedef struct rr_entry_t {
    uint16_t originator_id;
    uint8_t  data[RR_TABLE_MAX_DATA_LEN];
    size_t   data_len;
    int16_t  deadline;
    struct   rr_entry_t *next;
} rr_entry_t;

typedef struct rr_table_t {
    rr_entry_t* entries;
    rr_entry_t* head_busy;
    rr_entry_t* tail_busy;
    rr_entry_t* head_free;
    rr_entry_t* cur_busy;
} rr_table_t;

void rr_table_init(rr_table_t* table, rr_entry_t* entries_array);

rr_entry_t* rr_table_find(rr_table_t* table, uint16_t originator_id);

bool rr_table_add(rr_table_t* table, uint16_t originator_id, uint8_t* data, size_t data_len);

bool rr_table_remove(rr_table_t* table, uint16_t originator_id);

bool rr_table_contains(rr_table_t* table, uint16_t originator_id);

/** Return true if the table has no free entry left */
bool rr_table_is_empty(rr_table_t* table);

void rr_table_update_deadlines(rr_table_t* table, int16_t current_slot);
rr_entry_t* rr_table_get_next(rr_table_t* table);

void rr_table_print(rr_table_t* table);

#endif // RR_TABLE_H

