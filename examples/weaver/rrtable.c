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
#include "rrtable.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "contiki.h"

void
rr_table_init(rr_table_t* table, rr_entry_t* entries_array)
{
    table->entries = entries_array;
    table->head_busy = NULL;
    table->tail_busy = NULL;
    table->head_free = table->entries; // point to the first element
    table->cur_busy  = NULL;
}

rr_entry_t*
rr_table_find(rr_table_t* table, uint16_t originator_id)
{
    if (table == NULL) return NULL;
    if (table->head_busy == NULL) return NULL;

    rr_entry_t *tmp = table->head_busy;
    for (; tmp != NULL; tmp = tmp->next) {
        if (tmp->originator_id == originator_id)
            break;
    }

    return tmp;
}

bool
rr_table_add(rr_table_t* table, uint16_t originator_id, uint8_t* data, size_t data_len)
{
    if (table == NULL) return false;
    if (table->head_free == NULL) return false;

    rr_entry_t *entry = rr_table_find(table, originator_id);
    if (entry != NULL) return false; // element already present

    if (data_len > 127) return false;
    if (data == NULL && data_len > 0) return false;

    // get a free entry
    entry = table->head_free;
    table->head_free = table->head_free->next;

    // fill in entry details
    entry->originator_id = originator_id;
    if (data != NULL) {
        memcpy(&entry->data, data, data_len);
    }
    entry->data_len = data_len;
    entry->deadline = -1;

    entry->next = NULL; // detach from free list
    if (table->tail_busy != NULL) {
        table->tail_busy->next = entry;
    }

    table->tail_busy = entry;
    if (table->head_busy == NULL) {
        table->head_busy = entry;

        // this is the first element, cur points to this
        table->cur_busy = entry;
    }
    return true;
}

bool
rr_table_remove(rr_table_t* table, uint16_t originator_id)
{
    if (table == NULL) return false;
    if (table->head_busy == NULL) return false;

    rr_entry_t *entry = rr_table_find(table, originator_id);
    if (entry == NULL) return false; // element not present

    // find previous element
    rr_entry_t *prev = table->head_busy;
    for (; prev != NULL; prev = prev->next) {
        if (prev->next == entry)
            break;
    }

    rr_entry_t *next_entry = entry->next;
    if (prev == NULL) { // removing the head
        table->head_busy = next_entry;
    }

    if (table->tail_busy == entry) { // remving the tail
        table->tail_busy = prev;
    }

    if (prev != NULL) {
        prev->next = next_entry;
    }

    // if removing the current entry, set the current to
    // the previous entry if exists, otherwise to the next (if still
    // doesn't exist, then the list is empty)
    if (table->cur_busy == entry) {
        if (prev != NULL) {
            table->cur_busy = prev;
        } else {
            table->cur_busy = next_entry;
        }
    }

    // attach removed entry to free list
    entry->next = table->head_free;
    table->head_free = entry;
    return true;
}

bool
rr_table_contains(rr_table_t* table, uint16_t originator_id)
{
    rr_entry_t *entry = rr_table_find(table, originator_id);
    return entry != NULL;
}

bool
rr_table_is_empty(rr_table_t* table)
{
    // return table->cur_busy == NULL;
    return ((table->head_busy == table->tail_busy) && (table->head_busy == NULL));
}

void
rr_table_print(rr_table_t* table)
{
    if (table == NULL) return;
    if (table->head_busy == NULL) return;

    rr_entry_t *tmp = table->head_busy;
    for (; tmp != NULL; tmp = tmp->next) {
        if (tmp == table->cur_busy)
            printf("S %"PRIu16", C %d  <--", tmp->originator_id, tmp->deadline);
        else
            printf("S %"PRIu16", C %d", tmp->originator_id, tmp->deadline);

    }
}

void
rr_table_update_deadlines(rr_table_t* table, int16_t current_slot) {
    if (table == NULL) return;
    if (table->head_busy == NULL) return;

    rr_entry_t *tmp = table->head_busy;
    for (; tmp != NULL; tmp = tmp->next) {
        if (tmp->deadline != -1 && current_slot > tmp->deadline) {
            tmp->deadline = -1;              // flag the need for the pkt to be tx again
        }
    }
}

rr_entry_t*
rr_table_get_next(rr_table_t* table)
{
    if (table == NULL) return NULL;
    if (table->cur_busy == NULL) return NULL;

    if (table->cur_busy->deadline == -1)
        return table->cur_busy;

    rr_entry_t *tmp = table->cur_busy->next;
    if (tmp == NULL)
        tmp = table->head_busy; // set a circular behaviour

    bool found = false;
    while (tmp != table->cur_busy) {
        if (tmp->deadline == -1) {
            found = true;
            break;
        }
        tmp = tmp->next;
        if (tmp == NULL) {
            tmp = table->head_busy;
        }
    }
    if (found) {
        table->cur_busy = tmp;
        return tmp;
    }
    return NULL;
}

