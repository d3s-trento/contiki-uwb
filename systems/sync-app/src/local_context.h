#ifndef LOCAL_CONTEXT_H_
#define LOCAL_CONTEXT_H_

#include <stdint.h>

struct local_context_t {
  /** Epoch */
  uint16_t epoch;

  /** At which hop distance are we from the sink */
  uint8_t hop_distance;

  uint8_t cumulative_failed_synchronizations;

  bool seen_event;
};

#endif
