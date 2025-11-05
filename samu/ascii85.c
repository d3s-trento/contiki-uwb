#include "ascii85.h"

#include <stdlib.h>

#define ASCII85_INSUFFICIENT_DSTLEN -1
#define ASCII85_INVALID_SRC -2
#define ASCII85_INVALID_DST -3

#define ASCII85_BASE_CHAR 33U

int32_t ascii85_encode(uint8_t *const dst, uint32_t max_dst_len,
                       const uint8_t *const src, uint32_t src_len) {
  if (max_dst_len == 0) {
    return ASCII85_INVALID_DST;
  }

  if (dst == NULL) {
    return ASCII85_INVALID_DST;
  }

  if (src == NULL) {
    return ASCII85_INVALID_SRC;
  }

  if (src_len > INT32_MAX - 3) {
    return ASCII85_INSUFFICIENT_DSTLEN;
  }

  if ((src_len + 3) / 4 > INT32_MAX / 5) {
    return ASCII85_INSUFFICIENT_DSTLEN;
  }

  if (((src_len + 3) / 4) * 5 >= max_dst_len) {
    return ASCII85_INSUFFICIENT_DSTLEN;
  }

  int32_t out_len = 0;

  uint32_t complete_chunks = (src_len + 3) / 4;

  uint32_t i;

  for (i = 0; i < complete_chunks; ++i) {
    uint8_t b0 = src[i * 4 + 0];
    uint8_t b1 = (i * 4 + 1) > src_len ? 0 : src[i * 4 + 1];
    uint8_t b2 = (i * 4 + 2) > src_len ? 0 : src[i * 4 + 2];
    uint8_t b3 = (i * 4 + 3) > src_len ? 0 : src[i * 4 + 3];

    uint32_t chunk = b3 | (((uint32_t)b2) << 8) | (((uint32_t)b1) << 16) |
                     (((uint32_t)b0) << 24);

    if (chunk == 0) {
      dst[out_len] = 'z';
      out_len++;
    } else {
      dst[out_len + 4] = (chunk % 85) + ASCII85_BASE_CHAR;
      chunk /= 85;

      dst[out_len + 3] = (chunk % 85) + ASCII85_BASE_CHAR;
      chunk /= 85;

      dst[out_len + 2] = (chunk % 85) + ASCII85_BASE_CHAR;
      chunk /= 85;

      dst[out_len + 1] = (chunk % 85) + ASCII85_BASE_CHAR;
      chunk /= 85;

      // Should already be smaller than 85
      dst[out_len] = chunk + ASCII85_BASE_CHAR;

      out_len += 5;
    }
  }

  dst[out_len] = '\0';
  return out_len;
}
