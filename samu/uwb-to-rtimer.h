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
#ifndef UWB_TO_RTIMER_H
#define UWB_TO_RTIMER_H

#include "rtimer.h"
#include <stdint.h>

#ifndef UTR_MIN_SMOOTHING
#define UTR_MIN_SMOOTHING 1
#endif

#ifndef UTR_MAX_SMOOTHING
#define UTR_MAX_SMOOTHING 20000
#endif

#ifndef UTR_MAX_PPM
#define UTR_MAX_PPM 90
#endif

#ifndef THEORETICAL_UTR_CLOCK
#define THEORETICAL_UTR_CLOCK ((uint32_t)(7617.187500 * (1 << 17)))
#endif

void utr_init();
void utr_start_epoch(uint32_t uwb_timestamp, rtimer_clock_t rtimer_timestamp);
void utr_end_epoch(uint32_t uwb_timestamp, rtimer_clock_t rtimer_timestamp);
rtimer_clock_t utr_uwb_to_rtick(uint32_t uwb);
uint64_t utr_uwb_to_rtick_ext(uint32_t uwb);
uint32_t utr_get_ratio_u32();

#endif
