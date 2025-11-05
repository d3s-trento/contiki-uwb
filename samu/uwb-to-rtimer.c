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
 * \file Synchronized Action Manager for UWB (SAMU) uwb to rtimer estimation APIs
 *
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */
#include "uwb-to-rtimer.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#define LOG_PREFIX "utr"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#define MIN_UWB_IN_RTICK                                                       \
  ((uint32_t)(THEORETICAL_UTR_CLOCK * (1 - UTR_MAX_PPM * 1e-6)))
#define MAX_UWB_IN_RTICK                                                       \
  ((uint32_t)(THEORETICAL_UTR_CLOCK * (1 + UTR_MAX_PPM * 1e-6)))

#ifndef RESTART_LOGS
#define RESTART_LOGS 0
#endif

struct utr_context_t {
  bool initilized;
  bool theoretical;

  uint32_t prev_uwb_timestamp;
  rtimer_clock_t prev_rtimer_timestamp;

  uint32_t smoothing_factor;
  uint32_t uwb_in_rtick;
};

#define UTR_CONTEXT_INIT                                                       \
  ((struct utr_context_t){.initilized = false,                                 \
                          .theoretical = true,                                 \
                          .prev_uwb_timestamp = 0,                             \
                          .prev_rtimer_timestamp = 0,                          \
                          .smoothing_factor = UTR_MIN_SMOOTHING,               \
                          .uwb_in_rtick = THEORETICAL_UTR_CLOCK})

static struct utr_context_t status = UTR_CONTEXT_INIT;

void utr_init() { status = UTR_CONTEXT_INIT; }

void utr_start_epoch(uint32_t uwb_timestamp, rtimer_clock_t rtimer_timestamp) {
  status.prev_uwb_timestamp = uwb_timestamp;
  status.prev_rtimer_timestamp = rtimer_timestamp;
  status.initilized = true;
}

void utr_end_epoch(uint32_t uwb_timestamp, rtimer_clock_t rtimer_timestamp) {
#define IGNORE_EPOCH 500
#define SMOOTHENING_EPOCH 15000
  if (!status.initilized) {
    ERR("utr_start_epoch should be called first");
    status.initilized = false;
    return;
  }

  uint32_t rtimer_diff =
      RTIMER_CLOCK_DIFF(rtimer_timestamp, status.prev_rtimer_timestamp);

  if (rtimer_diff == 0) {
    WARN("rtimer diff is 0");
    status.prev_uwb_timestamp = uwb_timestamp;
    status.prev_rtimer_timestamp = rtimer_timestamp;
    return;
  }

  uint32_t diff_uwb = uwb_timestamp - status.prev_uwb_timestamp;
  uint64_t extended_diff_uwb = ((uint64_t)diff_uwb) << 32;

  if (rtimer_diff < IGNORE_EPOCH) {
    status.prev_uwb_timestamp = uwb_timestamp;
    status.prev_rtimer_timestamp = rtimer_timestamp;
    return;
  }

  uint64_t uwb_in_rtick = extended_diff_uwb / rtimer_diff;

  if (rtimer_diff < SMOOTHENING_EPOCH) {
    // If the expected period is short, make weight it so that the noise in the
    // measurement isn't higher than the threshold for the following filter
    uint32_t i = ((SMOOTHENING_EPOCH + (rtimer_diff - 1)) / rtimer_diff) + 1;

    uwb_in_rtick =
        (uwb_in_rtick + (i * (((uint64_t)status.uwb_in_rtick) << 15)) +
         (i + 1) / 2) /
        (i + 1);
  }

  WARNIF(uwb_in_rtick == 0);

  if (uwb_in_rtick >= (((uint64_t)1) << 49)) {
    WARN("Value too big");
    status.prev_uwb_timestamp = uwb_timestamp;
    status.prev_rtimer_timestamp = rtimer_timestamp;
    return;
  }

#if RESTART_LOGS
  printf("[utr %" PRIu32 "] raw uwb_in_rtick %" PRIu64 "\n", logging_context,
         uwb_in_rtick);
#endif

  // If the drift calculated is outside what is reasonable, ignore it
  // (ignores calculations on synchronized epochs with an hidden epoch in
  // between)
  if ((uwb_in_rtick >= (((uint64_t)MIN_UWB_IN_RTICK) << 15)) &&
      (uwb_in_rtick <= (((uint64_t)MAX_UWB_IN_RTICK) << 15))) {
    // Exponential mean (using uint64_t) with 1/5
    if (status.theoretical) {
      status.theoretical = false;
    } else {
      WARNIF(status.smoothing_factor == 0);

      uint32_t actual_smoothing = status.smoothing_factor / 10;
      if (actual_smoothing < 2)
        actual_smoothing = 2;

      uwb_in_rtick = (uwb_in_rtick + (((uint64_t)status.uwb_in_rtick) << 15) *
                                         (actual_smoothing - 1)) /
                     actual_smoothing;
      status.smoothing_factor = (status.smoothing_factor >= UTR_MAX_SMOOTHING)
                                    ? UTR_MAX_SMOOTHING
                                    : (status.smoothing_factor + 1);
    }

    // Reduce to a 32bit fixed point [15].[17]
    status.uwb_in_rtick = uwb_in_rtick >> 15;
  } else {
    // status.smoothing_factor = status.smoothing_factor*3/4;
#if RESTART_LOGS
    printf("[utr %" PRIu32 "] Ignored uwb_in_rtick %" PRIu64 " %lu %lu\n",
           logging_context, uwb_in_rtick, diff_uwb, rtimer_diff);
#endif
  }

  WARNIF(status.uwb_in_rtick == 0);

#if RESTART_LOGS
  printf("[utr %" PRIu32 "] current uwb_in_rtick %" PRIu32 "\n",
         logging_context, status.uwb_in_rtick);
#endif

  status.prev_uwb_timestamp = uwb_timestamp;
  status.prev_rtimer_timestamp = rtimer_timestamp;
  status.initilized = false;
}

rtimer_clock_t utr_uwb_to_rtick(uint32_t uwb) {
  uint64_t extended_uwb = uwb;
  extended_uwb <<= 32;

  //*
  extended_uwb /= status.uwb_in_rtick;
  /*/
  extended_us /=  THEORETICAL_UTR_CLOCK;
  //*/

  extended_uwb >>= 15;
  WARNIF(extended_uwb == 0);
  return extended_uwb;
}

uint64_t utr_uwb_to_rtick_ext(uint32_t uwb) {
  uint64_t extended_uwb = uwb;
  extended_uwb <<= 32;

  //*
  extended_uwb /= status.uwb_in_rtick;
  /*/
  extended_us /=  THEORETICAL_UTR_CLOCK;
  //*/

  WARNIF(extended_uwb == 0);
  return extended_uwb;
}

uint32_t utr_get_ratio_u32() { return status.uwb_in_rtick; }
