/*
 * Copyright (c) 2019, University of Trento.
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
 * \author
 *      Diego Lobba <diego.lobba@gmail.com>
 */
#ifndef DW1000_STATETIME_H
#define DW1000_STATETIME_H

#include <inttypes.h>
#include <stdbool.h>

/*
 * Define radio states that are useful to determine the actions
 * taken by the radio.
 */
typedef enum dw1000_state_t {
    DW1000_IDLE,
    DW1000_RX_AFTER_TX,
    DW1000_SCHEDULED_TX,
    DW1000_SCHEDULED_RX
} dw1000_state_t;

typedef struct dw1000_statetime_log_t {
    uint64_t idle_time_us;
    uint64_t rx_preamble_hunting_time_us;
    uint64_t rx_preamble_time_us;
    uint64_t rx_data_time_us;
    uint64_t tx_preamble_time_us;
    uint64_t tx_data_time_us;
} dw1000_statetime_log_t;

/* Define a context to track the time spent in each radio state.
 * Currently, the module support the following states:
 *
 * - RX
 * - TX
 * - IDLE
 * - Preamble hunting (listening without receiving data)
 *
 * When considering RX and TX, the module differentiates the time
 * spent RX/TX the preamble from the one spent for the PHR and PHY payload.
 */
typedef struct dw1000_statetime_context_t {
    dw1000_state_t state;

    uint64_t idle_time_us;
    uint64_t rx_preamble_hunting_time_us;
    uint64_t rx_preamble_time_us;
    uint64_t rx_data_time_us;
    uint64_t tx_preamble_time_us;
    uint64_t tx_data_time_us;

    bool     tracing;                   // true if tracing is active
    uint32_t schedule_32hi;             // the timestamp of the last scheduled function
                                        //   (starttx or rxenable 4ns precision)
    uint32_t last_idle_32hi;            // timestamp of last time the radio switched to idle


    bool     is_rx_after_tx;            // set to true if the previous rx operation was performed after tx
    uint32_t rx_delay_32hi;             // the amount of delay (4ns precision)
} dw1000_statetime_context_t;
/*---------------------------------------------------------------------------*/
/** \brief Initialize the statetime context.
 */
void dw1000_statetime_context_init();
/*---------------------------------------------------------------------------*/
/** \brief Start tracing statetime.
 */
void dw1000_statetime_start();
/*---------------------------------------------------------------------------*/
/** \brief Stop tracing statetime.
 */
void dw1000_statetime_stop();
/*---------------------------------------------------------------------------*/
/** \brief Retrieve the statetime context.
 */
dw1000_statetime_context_t* dw1000_statetime_get_context();

void dw1000_statetime_log(dw1000_statetime_log_t* entry);
/*---------------------------------------------------------------------------*/
/** \brief Print dwell times on each state.
 */
void dw1000_statetime_print();
/*---------------------------------------------------------------------------*/
/** \brief Set the last timestamp where the radio was IDLE.
 */
void dw1000_statetime_set_last_idle(const uint32_t ts_idle_32hi);
/*---------------------------------------------------------------------------*/
/** \brief Perform tracing after a successful frame reception.
 *
 * Distinguish between:
 *
 * 1. idle time: the time between last-idle time and the schedule rx time
 * 2. preamble hunting: the time between the rx schedule and the preamble reception
 * 3. data time
 *
 *  \param sfd_rx_32hi  sfd of the incoming rx.
 *  \param framelength  the length of the frame received.
 */
void dw1000_statetime_after_rx(const uint32_t sfd_rx_32hi, const uint16_t framelength);
/*---------------------------------------------------------------------------*/
/** \brief Perform tracing after a successful transmission.
 *
 * Distinguish between:
 *
 * 1. idle time: the time between last-idle time and the preamble tx time
 * 2. preamble
 * 3. data time
 *
 *  \param sfd_rx_32hi  sfd of the tx.
 *  \param framelength  the length of the frame transmitted.
 */
void dw1000_statetime_after_tx(const uint32_t sfd_tx_32hi, const uint16_t framelength);
/*---------------------------------------------------------------------------*/
void dw1000_statetime_schedule_tx(const uint32_t schedule_tx_32hi);
/*---------------------------------------------------------------------------*/
/** \brief Used when scheduling tx and automatic switch to rx
 */
void dw1000_statetime_schedule_txrx(const uint32_t schedule_tx_32hi,
        const uint32_t rx_delay_uus);
/*---------------------------------------------------------------------------*/
void dw1000_statetime_schedule_rx(const uint32_t schedule_rx_32hi);
/*---------------------------------------------------------------------------*/
#endif /* DW1000_STATETIME_H */
