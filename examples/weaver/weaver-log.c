#include <string.h>
#include <stddef.h>
#include <inttypes.h>

#include "trex.h"
#include "weaver-log.h"

#define LOG_PREFIX "E"  // epoch
#include "logging.h"

void weaver_log_append(weaver_log_t *entry);
void weaver_log_print();
void weaver_log_init();

#define WEAVER_LOGS_MAX 100
static weaver_log_t weaver_slot_logs[WEAVER_LOGS_MAX];
static size_t next_log = 0;

void
weaver_log_init() {
    next_log = 0;
}

void
weaver_log_append(weaver_log_t *entry) {
    if (next_log < WEAVER_LOGS_MAX) {
        weaver_slot_logs[next_log] = *entry;
        next_log++;
    }
}

void 
weaver_log_print() {
    weaver_log_t *s;
    char stat_label[2] = "";

    for (s = weaver_slot_logs; s < weaver_slot_logs + next_log; s++) {

        switch (s->slot_status) {
            case TREX_TX_DONE:
                snprintf(stat_label, 2, "T"); break; // Transmitted
            case TREX_RX_SUCCESS:
                snprintf(stat_label, 2, "R"); break; // Received
            case TREX_RX_TIMEOUT:
                snprintf(stat_label, 2, "L"); break; // Late
            case TREX_RX_ERROR:
                snprintf(stat_label, 2, "E"); break; // Error
            case TREX_RX_MALFORMED:
                snprintf(stat_label, 2, "B"); break; // Bad
            case -1:                                 // TSM_LOG_STATUS_RX_WITH_SYNCH
                snprintf(stat_label, 2, "Y"); break; // received and resYnchronised
            default:
                snprintf(stat_label, 2, "#");        // unknown (should not happen)
        }
        printf(LOG_PREFIX " %u, I %"PRIi16", L %s, D %"PRIu8", S %"PRIu16", H %"PRIu16", A 0x%"PRIx64", B 0x%"PRIx64"\n",
                logging_context, s->idx, stat_label, s->node_dist, s->originator_id, s->lhs, s->acked, s->buffer);
    }
}
