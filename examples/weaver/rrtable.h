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
    // uint8_t     max_counter;
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
