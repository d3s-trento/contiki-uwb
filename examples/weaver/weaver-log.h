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
