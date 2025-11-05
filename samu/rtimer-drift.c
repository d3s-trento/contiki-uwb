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
#include "rtimer-drift.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "rtimer.h"

#define LOG_PREFIX "rde"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#include "print-def.h"

// TODO: Should check for rtimer_clock_t size

#define MIN_US_IN_RTICK                                                        \
  ((uint32_t)(THEORETICAL_RDE_CLOCK * (1 - RDE_MAX_PPM * 1e-6)))
#define MAX_US_IN_RTICK                                                        \
  ((uint32_t)(THEORETICAL_RDE_CLOCK * (1 + RDE_MAX_PPM * 1e-6)))

#ifndef RESTART_LOGS
#define RESTART_LOGS 0
#endif

static struct {
  bool initialized;
  bool theoretical;

  uint32_t us_in_rtick;

  rtimer_clock_t prev_rtimer;
  uint64_t prev_epoch_length;

  uint32_t expected_period_us;

  uint32_t smoothing_factor;
} status;

void rde_init() {
  status.initialized = false;
  status.theoretical = true;

  status.us_in_rtick = THEORETICAL_RDE_CLOCK;

  status.prev_rtimer = 0;
  status.prev_epoch_length = 0;

  status.expected_period_us = 0;
  status.smoothing_factor = RDE_MIN_SMOOTHING;
}

void rde_set_expected_time(uint32_t expected_period_us) {
  status.expected_period_us = expected_period_us;
}

void rde_add(rtimer_clock_t rtimer_now, uint64_t epoch_length) {
  if (!status.initialized) {
    status.initialized = true;

    status.prev_rtimer = rtimer_now;
    status.prev_epoch_length = epoch_length;
    return;
  }

  uint32_t rtimer_diff = RTIMER_CLOCK_DIFF(rtimer_now, status.prev_rtimer);

  if (rtimer_diff == 0) {
    WARN("rtimer diff is 0");
    status.prev_rtimer = rtimer_now;
    status.prev_epoch_length = epoch_length;
    return;
  }

  uint64_t rtimer_diff_ext = (((uint64_t)rtimer_diff) << 6) -
                             ((uint64_t)(epoch_length >> 9)) +
                             ((uint64_t)(status.prev_epoch_length >> 9));

  uint32_t diff_us = status.expected_period_us;
  uint64_t extended_diff_us = ((uint64_t)diff_us) << 32;

  uint64_t us_in_rtick = extended_diff_us / rtimer_diff_ext;
  us_in_rtick = us_in_rtick << 6;

  if (status.expected_period_us < 2000000) {
    // If the expected period is short, make weight it so that the noise in the
    // measurement isn't higher than the threshold for the following filter
    uint32_t i = ((2000000 + (status.expected_period_us - 1)) /
                  status.expected_period_us) +
                 1;

    us_in_rtick = (us_in_rtick + (i * (((uint64_t)status.us_in_rtick) << 7)) +
                   (i + 1) / 2) /
                  (i + 1);
  }

  WARNIF(us_in_rtick == 0);

  if (us_in_rtick >= (((uint64_t)1) << 39)) {
    WARN("Value too big");
    status.prev_rtimer = rtimer_now;
    status.prev_epoch_length = epoch_length;
    return;
  }

  // If the drift calculated is outside what is reasonable, ignore it
  // (ignores calculations on synchronized epochs with an hidden epoch in
  // between)
  if ((us_in_rtick >= (((uint64_t)MIN_US_IN_RTICK) << 7)) &&
      (us_in_rtick <= (((uint64_t)MAX_US_IN_RTICK) << 7))) {
    // Exponential mean (using uint64_t) with 1/5
    if (status.theoretical) {
      status.theoretical = false;
    } else {
      WARNIF(status.smoothing_factor == 0);

      uint32_t actual_smoothing = status.smoothing_factor / 10;
      if (actual_smoothing < 2)
        actual_smoothing = 2;

      us_in_rtick = (us_in_rtick + (((uint64_t)status.us_in_rtick) << 7) *
                                       (actual_smoothing - 1)) /
                    actual_smoothing;
      status.smoothing_factor = (status.smoothing_factor >= RDE_MAX_SMOOTHING)
                                    ? RDE_MAX_SMOOTHING
                                    : (status.smoothing_factor + 1);
    }

    // Reduce to a 32bit fixed point [7].[25]
    status.us_in_rtick = us_in_rtick >> 7;
  } else {
    // status.smoothing_factor = status.smoothing_factor*3/4; //
    // RDE_MIN_SMOOTHING;
#if RESTART_LOGS
    printf("[rde %" PRIu32 "] Ignored us_in_rtick %" PRIu64 " %" PRIu32
           " %" PRIu64 " %" PRIu64 "\n",
           logging_context, us_in_rtick, rtimer_diff, epoch_length,
           status.prev_epoch_length);
#endif
  }

  WARNIF(status.us_in_rtick == 0);

#if RESTART_LOGS
  printf("[rde %" PRIu32 "] current us_in_rtick %" PRIu32 "\n", logging_context,
         status.us_in_rtick);
#endif

  status.prev_rtimer = rtimer_now;
  status.prev_epoch_length = epoch_length;
}

void rde_skip() {
  status.initialized = false;

  // status.smoothing_factor /= 2;
  // if (status.smoothing_factor < RDE_MIN_SMOOTHING) {
  // 	status.smoothing_factor = RDE_MIN_SMOOTHING;
  // }
}

rtimer_clock_t rde_us_to_rtick(uint32_t us) {
  uint64_t extended_us = us;
  extended_us <<= 32;

  //*
  extended_us /=
      status
          .us_in_rtick; // (Ideally it would be (status.us_in_rtick / (1 << 25))
  /*/
  extended_us /=  THEORETICAL_RDE_CLOCK; // (Ideally it would be
  (status.us_in_rtick / (1 << 25))
  //*/

  extended_us >>= 7;
  WARNIF(extended_us == 0);
  return extended_us;
}

uint64_t rde_us_to_rtick_ext(uint32_t us) {
  uint64_t extended_us = us;
  extended_us <<= 32;

  //*
  extended_us /=
      status
          .us_in_rtick; // (Ideally it would be (status.us_in_rtick / (1 << 25))
  /*/
  extended_us /=  THEORETICAL_RDE_CLOCK; // (Ideally it would be
  (status.us_in_rtick / (1 << 25))
  //*/

  WARNIF(extended_us == 0);
  return extended_us;
}

uint32_t rde_get_ratio_u32() { return status.us_in_rtick; }
