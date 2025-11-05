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
 * \file Synchronized Action Manager for UWB (SAMU) rtimer estimation APIs
 *
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */
#ifndef RTIMER_DRIFT_H
#define RTIMER_DRIFT_H

#include "rtimer.h"
#include <stdint.h>

#ifndef RDE_MIN_SMOOTHING
#define RDE_MIN_SMOOTHING 1
#endif

#ifndef RDE_MAX_SMOOTHING
#define RDE_MAX_SMOOTHING 20000
#endif

#ifndef RDE_MAX_PPM
#define RDE_MAX_PPM 200
#endif

#ifndef THEORETICAL_RDE_CLOCK
#define THEORETICAL_RDE_CLOCK ((uint32_t)(30.51757813 * (1 << 25)))
#endif

void rde_init();
void rde_set_expected_time(uint32_t expected_period_us);

void rde_add(rtimer_clock_t rtimer_now, uint64_t epoch_length);
void rde_skip();

rtimer_clock_t rde_us_to_rtick(uint32_t us);
uint64_t rde_us_to_rtick_ext(uint32_t us);
uint32_t rde_get_ratio_u32();

#endif
