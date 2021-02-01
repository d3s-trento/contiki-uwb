/*
 * Copyright (c) 2018, University of Trento.
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
 * \file
 *    CIR Reading and Printing Functions
 *
 * \author
 *    Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 *    Timofei Istomin <tim.ist@gmail.com>
 */


#include "dw1000-cir.h"
#include "dw1000-config.h"
#include "contiki.h"
#include "deca_regs.h"
#include "deca_device_api.h"
/*---------------------------------------------------------------------------*/
#include <stdio.h>
/*---------------------------------------------------------------------------*/

#define LOG_LEVEL LOG_ERR
#include "logging.h"
/*---------------------------------------------------------------------------*/

#define CIR_PRINT_STEP 128

/*---------------------------------------------------------------------------*/

/* Read max n_samples samples from the CIR accumulator, starting at index s1 or at the
 * index of the first ray. Call after packet reception and before re-enabling listening.
 *
 * Params
 *  - s1 [in]         starting index in the CIR accumulator, or DW1000_CIR_FIRST_RAY 
 *                    to start reading at the first ray index, as detected by the radio
 *  - n_samples [in]  max number of samples to read
 *  - samples [out]   buffer to read the CIR data into. MUST be big enough to contain 
 *                    n_samples+1 records. The first record (samples[0]) will 
 *                    contain the accumulator index the reading started from.
 *
 * Returns the actual number of samples read.
 *
 * NB: the samples buffer must be big enough to contain n_samples+1 records. The CIR data starts
 *     at index 1 of the samples buffer.
 */
uint16_t dw1000_read_cir(int16_t s1, uint16_t n_samples, dw1000_cir_sample_t* samples) {
  if (s1 == DW1000_CIR_FIRST_RAY) {
    s1 = dwt_read16bitoffsetreg(LDE_IF_ID, LDE_PPINDX_OFFSET);
  }

  uint16_t max_samples = (dw1000_get_current_cfg()->prf == DWT_PRF_64M) ? 
                           DW1000_CIR_LEN_PRF64 : 
                           DW1000_CIR_LEN_PRF16;

  if (s1 >= max_samples) {
    ERR("Invalid index");
    return 0;
  }

  // adjusting the length to read w.r.t. the accumulator tail size
  if (s1 + n_samples >= max_samples) {
    n_samples = max_samples - s1;
  }

  printf("CIR[%u:%u] ", s1, n_samples);

  uint16_t start_byte_idx = s1 * DW1000_CIR_SAMPLE_SIZE;
  uint16_t len_bytes      = n_samples * DW1000_CIR_SAMPLE_SIZE;
  
  // we start reading into the buffer one byte before the second 4-byte word
  // because of the way dwt_readaccdata() works (it always skips the first byte of the buffer).
  dwt_readaccdata(((uint8_t*)samples) + 3, len_bytes, start_byte_idx);

  samples[0] = s1;

  return n_samples;
}


/* Print max n_samples samples from the CIR accumulator, starting at index s1 or at the
 * index of the first ray. Call after packet reception and before re-enabling listening.
 *
 * Params
 *  - s1 [in]              starting index in the CIR accumulator, or DW1000_CIR_FIRST_RAY 
 *                         to start reading at the first ray index, as detected by the radio
 *  - n_samples [in]       max number of samples to read
 *  - human_readable [in]  print in human-readable form (a+bj) instead of plain hex
 *
 * Returns the actual number of samples printed.
 */
uint16_t dw1000_print_cir_samples_from_radio(int16_t s1, uint16_t n_samples, bool human_readable) {
  uint8_t buf[CIR_PRINT_STEP + 1];

  if (s1 == DW1000_CIR_FIRST_RAY) {
    s1 = dwt_read16bitoffsetreg(LDE_IF_ID, LDE_PPINDX_OFFSET);
  }

  uint16_t max_samples = (dw1000_get_current_cfg()->prf == DWT_PRF_64M) ? 
                           DW1000_CIR_LEN_PRF64 : 
                           DW1000_CIR_LEN_PRF16;

  if (s1 >= max_samples) {
    ERR("Invalid index");
    return 0;
  }

  // adjusting the length to read w.r.t. the accumulator tail size
  if (s1 + n_samples >= max_samples) {
    n_samples = max_samples - s1;
  }

  printf("CIR[%u:%u] ", s1, n_samples);

  uint16_t start_byte_idx = s1 * DW1000_CIR_SAMPLE_SIZE;
  uint16_t len_bytes      = n_samples * DW1000_CIR_SAMPLE_SIZE;

  uint16_t read_bytes = 0;
  uint16_t idx = start_byte_idx;

  while (read_bytes < len_bytes) {
    uint16_t chunk_size = len_bytes - read_bytes;
    if (chunk_size > CIR_PRINT_STEP) {
      chunk_size = CIR_PRINT_STEP;
    }
    dwt_readaccdata(buf, chunk_size + 1, idx);
    read_bytes += chunk_size;
    idx += chunk_size;

    if (human_readable) {
      for(int k = 1; k < chunk_size + 1; k = k + 4) {
        int16_t a = (((uint16_t)buf[k + 1]) << 8) | buf[k];
        int16_t b = (((uint16_t)buf[k + 3]) << 8) | buf[k + 2];
        if(b >= 0) {
          printf("%d+%dj,", a, b);
        } else {
          printf("%d%dj,", a, b);
        }
      }
    }
    else {
      for(uint16_t k = 1; k < chunk_size + 1; k++) {
        printf("%02x", buf[k]);
      }
    }
  }

  printf("\n");
  return n_samples;
}

/* Print whole CIR. Call after packet reception and before re-enabling listening.
 *
 * Params
 *  - human_readable [in]   print in human-readable form (a+bj) instead of plain hex
 *
 * Returns the actual number of samples printed.
 */
uint16_t dw1000_print_cir_from_radio(bool human_readable) {
  // print all from the beginning
  return dw1000_print_cir_samples_from_radio(0, DW1000_CIR_MAX_LEN, human_readable);
}

