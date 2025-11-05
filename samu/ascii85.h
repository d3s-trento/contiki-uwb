#ifndef ASCII85_H
#define ASCII85_H

#include <stdint.h>

int32_t ascii85_encode(uint8_t *const dst, uint32_t max_dst_len,
                       const uint8_t *const src, uint32_t src_len);

#endif
