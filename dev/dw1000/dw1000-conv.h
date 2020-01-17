/*
 * Copyright (c) 2017, University of Trento.
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

/**
 * \file
 *      Contiki DW1000 Device Time Conversion
 *
 * \author
 *      Davide Vecchia <davide.vecchia@unitn.it>
 */

 #ifndef DW1000_CONV_H
 #define DW1000_CONV_H

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu */
#define UUS_TO_DWT_TIME 65536LL

/* UWB microsecond (uus) to device time unit for the 32-bits timestamp */
#define UUS_TO_DWT_TIME_32 256

/* UWB microsecond (uus) to µs conversion */
#define UUS_TO_US 1.0256410256 // 512 / 499.2

/* UWB tick value in ns (31 bit timestamp) */
#define DWT_TICK_TO_NS_31 8.0128205128 // (2^9 * 512 / 499.2) / UUS_TO_DWT_TIME * 1000

/* UWB tick value in ns (32 bit timestamp) */
#define DWT_TICK_TO_NS_32 4.0064102564 // (2^8 * 512 / 499.2) / UUS_TO_DWT_TIME * 1000

/* UWB tick value in ns (64 bit timestamps) */
#define DWT_TICK_TO_NS_64 0.0156500400641 // (2^0 * 512 / 499.2) / UUS_TO_DWT_TIME * 1000
#endif /* DW1000_CONV_H */
