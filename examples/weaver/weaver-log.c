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
#include <string.h>
#include <stddef.h>
#include <inttypes.h>

#include "trex.h"
#include "weaver-log.h"

#define LOG_PREFIX "E"  // epoch
#include "logging.h"

#define WEAVER_LOGS_MAX 100
static weaver_log_t weaver_slot_logs[WEAVER_LOGS_MAX];
static size_t next_log = 0;

void
weaver_log_init()
{
    next_log = 0;
}

void
weaver_log_append(weaver_log_t *entry)
{
    if (next_log < WEAVER_LOGS_MAX) {
        weaver_slot_logs[next_log] = *entry;
        next_log++;
    }
}

void
weaver_log_print()
{
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

