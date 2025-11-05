/*
 * Copyright (c) 2025, University of Trento.
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
 * \file Synchronized Action Manager for UWB (SAMU) Deep-sleep API for Contiki
 *
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */
#ifndef CONTIKI_SAMU_H
#define CONTIKI_SAMU_H

#include <stdbool.h>
#include <stdint.h>

#include "rtimer.h"
#include "samu.h"

typedef void (*epoch_start_cb)(void);
typedef void (*upper_layer_cb)(void);

void contiki_samu_init(epoch_start_cb es_cb, epoch_start_cb ul_cb,
                       struct samu_context_t *samu_tref);
void contiki_samu_start();
int contiki_samu_goto_sleep();
void contiki_samu_epoch_restart();

rtimer_clock_t contiki_samu_us_to_rtick(uint32_t us);

#endif
