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
#include "watchdog.h"
/*---------------------------------------------------------------------------*/
#if PRINTF_OVER_RTT
#include "SEGGER_RTT.h"
#elif CONTIKI_TARGET_EVB1000
#include "deca_usb.h"
#endif
#include <stdio.h>
/*---------------------------------------------------------------------------*/

#define LOG_LEVEL LOG_ERR
#include "logging.h"
/*---------------------------------------------------------------------------*/

#define SPI_READ_LIMIT 0 // on some platforms SPI cannot read all CIR at once. Zero means no limit.
#define CIR_PRINT_STEP 128 // printing in small portions to avoid creating huge buffers

/*---------------------------------------------------------------------------*/

/* Read max n_samples samples from the CIR accumulator, starting at index s1.
 * Call after packet reception and before re-enabling listening.
 *
 * Params
 *  - s1 [in]         sample index in the CIR accumulator to start reading from 
 *  - n_samples [in]  max number of samples to read
 *  - samples [out]   buffer to read the CIR data into. MUST be big enough to contain 
 *                    n_samples+1 records. The first record (samples[0]) will 
 *                    contain the start index (see below).
 *
 * Returns the actual number of samples read.
 *
 * samples[0] will contain the starting index of the acquired CIR.
 *
 * NB: the samples buffer must be big enough to contain n_samples+1 records. The CIR data starts
 *     at index 1 of the samples buffer.
 */
uint16_t dw1000_read_cir(int16_t s1, uint16_t n_samples, dw1000_cir_sample_t* samples) {
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

  uint16_t start_byte_idx = s1 * DW1000_CIR_SAMPLE_SIZE;
  uint16_t len_bytes      = n_samples * DW1000_CIR_SAMPLE_SIZE;
  
  uint16_t read_idx  = start_byte_idx;
  uint8_t* write_pos = (uint8_t*)&samples[1]; // we begin from index 1

#if (SPI_READ_LIMIT > 0)
  uint16_t read_bytes = 0;
  while (read_bytes < len_bytes) {
    uint16_t chunk_size = len_bytes - read_bytes;
    if (chunk_size > SPI_READ_LIMIT) {
      chunk_size = SPI_READ_LIMIT;
    }

    // we need to save one byte from the previous chunk because dwt_readaccdata() always
    // writes zero to the first byte of the current chunk
    uint8_t save_byte = *(write_pos-1);
    dwt_readaccdata(write_pos-1, chunk_size + 1, read_idx);
    *(write_pos-1) = save_byte;
    read_bytes += chunk_size;
    read_idx += chunk_size;
    write_pos += chunk_size;
  }
#else
  dwt_readaccdata(write_pos-1, len_bytes + 1, read_idx);
#endif
  samples[0] = s1;

  return n_samples;
}

/* Extract the real and complex parts as signed integers from the CIR sample.
 *
 * Params
 *  - sample [in]   single CIR sample
 *  - r [out]       real part
 *  - c [out]       complex part
 */
void dw1000_get_cir_sample_parts(const dw1000_cir_sample_t sample, int16_t* r, int16_t* c) {

  /* Read the sample into signed 16-bit integers */
  *r = sample & 0x0000ffff;
  *c = sample & 0xffff0000;
  // if(*r > 0x7fff) *r = *r - ((uint32_t)1 << 16);
  // if(*c > 0x7fff) *c = *c - ((uint32_t)1 << 16);
}

/* Print max n_samples samples from the CIR accumulator, starting at index s1.
 * Call after packet reception and before re-enabling listening.
 *
 * Params
 *  - s1 [in]              sample index in the CIR accumulator to start reading from
 *  - n_samples [in]       max number of samples to read
 *
 * Returns the actual number of samples printed.
 */
uint16_t dw1000_print_cir_samples_from_radio(int16_t s1, uint16_t n_samples) {
  uint8_t buf[CIR_PRINT_STEP + 1];
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
  uint16_t read_idx = start_byte_idx;

  while (read_bytes < len_bytes) {
    watchdog_periodic();
    uint16_t chunk_size = len_bytes - read_bytes;
    if (chunk_size > CIR_PRINT_STEP) {
      chunk_size = CIR_PRINT_STEP;
    }
    dwt_readaccdata(buf, chunk_size + 1, read_idx);
    read_bytes += chunk_size;
    read_idx += chunk_size;

    // TODO: call the dw1000_print_cir_hex instead
    for(uint16_t k = 1; k < chunk_size + 1; k++) {
      printf("%02x", buf[k]);
    }
  }

  printf("\n");
  return n_samples;
}

/* Print whole CIR. Call after packet reception and before re-enabling listening.
 *
 * Returns the actual number of samples printed.
 */
uint16_t dw1000_print_cir_from_radio() {
  // print all from the beginning
  return dw1000_print_cir_samples_from_radio(0, DW1000_CIR_MAX_LEN);
}

/* Print CIR buffer in hex.
 *
 * NB! This is a blocking function. If USB output is used, it will block
 * in busy waiting until the whole CIR is printed. If the host is too slow
 * to poll USB or it is not polling, or it is not connected, this
 * function will **BLOCK FOREVER**. Probably, watchdog will reboot the device.
 */
void dw1000_print_cir_hex(dw1000_cir_sample_t* cir, uint16_t n_samples) {
  static const char t[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                   'a', 'b', 'c', 'd', 'e', 'f' };

#if CONTIKI_TARGET_EVB1000
  fflush(0); // flush printf buffer to avoid data reordering
#endif

  char buf[9];
  for (int i=0; i<n_samples; i++) {
    uint8_t *p = (uint8_t*)(cir+i);
    buf[0] = t[*p >> 4];
    buf[1] = t[*p & 0xf];
    p++;
    buf[2] = t[*p >> 4];
    buf[3] = t[*p & 0xf];
    p++;
    buf[4] = t[*p >> 4];
    buf[5] = t[*p & 0xf];
    p++;
    buf[6] = t[*p >> 4];
    buf[7] = t[*p & 0xf];
    buf[8] = 0; // terminating zero
#if PRINTF_OVER_RTT
    int res, bytes_written;
    bytes_written = 0;
    do {
      res = SEGGER_RTT_Write(0, buf + bytes_written, 8 - bytes_written);
      bytes_written += res;
    } while (bytes_written < 8 && res >= 0);
    if (res < 0) {
      break;
    }
#elif CONTIKI_TARGET_EVB1000
    uint16_t res;
    do {
      res = DW_VCP_DataTx((uint8_t*)buf, 8);
      if (res != USBD_OK) clock_wait(1); // 1 ms
    } while (res != USBD_OK);
#else
    printf(buf);
#endif
  }
#if PRINTF_OVER_RTT
  SEGGER_RTT_Write(0, "\n", 1);
#elif CONTIKI_TARGET_EVB1000
  uint16_t res;
  do {
    res = DW_VCP_DataTx((uint8_t*)"\n", 1);
    if (res != USBD_OK) clock_wait(1); // 1 ms
  } while (res != USBD_OK);
#else
  printf("\n");
#endif
}
