/*
 * Copyright (c) 2020, University of Trento.
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
 * \file      Synchronized Action Manager for UWB (SAMU)
 *
 * TSM (code) authors:
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 *
 * SAMU and Flick (code) authors:
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */

#ifndef SAMU_RAL_STATUS_H
#define SAMU_RAL_STATUS_H

enum samu_ral_status {
  STATUS_NONE,  // there was no operation, so no status
  RX_SUCCESS,   // all good, packet data is loaded to the buffer
  RX_TIMEOUT,   // radio reported RX timeout
  RX_ERROR,     // radio reported RX error
  RX_MALFORMED, // packet was received but its format is incorrect
  TIMER_EVENT,  // RAL timer triggered
  TX_DONE,      // transmission has been performed
  FLICK_FALSE,
  FLICK_DETECTED,
  FLICK_TRUE,
  FLICK_ERROR,
};
#define SAM_RAL_STATUS_ENUM_MAX                                                \
  (FLICK_ERROR) // maximum value of enum samu_ral_status

#define SAMU_IS_RX_STATUS(status)                                              \
  ((status) >= RX_SUCCESS && (status) <= RX_MALFORMED)

#endif // SAMU_RAL_STATUS_H
