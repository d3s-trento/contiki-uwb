#ifndef LCG_GENERATOR_H
#define LCG_GENERATOR_H

#include PROJECT_CONF_H

#include <stdint.h>

#define R_NUMBER_NODES (64)

uint64_t r_bitmap(uint32_t target_epoch, uint8_t num_ones) {
    static uint32_t last_epoch = 0;
    static uint32_t r_number = R_NUMBER_SEED;

    uint64_t bitmap = 0;
    uint8_t i;

    while(last_epoch <= target_epoch) {
        bitmap = 0;
        i = 0;

        while (i<num_ones) {
            r_number = ((r_number * 1103515245U) + 12345U) & 0x7fffffff;

            uint64_t mask = (((uint64_t)1) << ((uint64_t) (r_number%R_NUMBER_NODES)));

            if (mask & R_NUMBER_EXCL) {
            } else if (bitmap & mask) {
            } else {
                bitmap |= mask;
                ++i;
            }
        }

        last_epoch++;
    }

    return bitmap;
}

#endif
