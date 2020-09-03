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

/**
 * \file
 *      Glossy implementation for DW1000 in Contiki OS
 *
 * \author
 *      Diego Lobba <diego.lobba@gmail.com>
 */

/**
 * \defgroup glossy Glossy API for the DW1000 SoC
 * \{
 * \file   glossy.h
 * \file   glossy.c
 */

#ifndef GLOSSY_H_
#define GLOSSY_H_

#ifndef GLOSSY_STATS
#define GLOSSY_STATS                    1
#endif  /* GLOSSY_STATS */

#include <inttypes.h>
#include <stdbool.h>
#include "sys/rtimer.h" // removed dependency on the whole contiki

#define GLOSSY_START_DELAY_US           450  // (measured for EVB1000, 6.8Mbps, 128 us preamble, it is CPU-dependent)
#define GLOSSY_START_DELAY_4NS          (GLOSSY_START_DELAY_US*1000/4)

enum {
    GLOSSY_UNKNOWN_INITIATOR = 0
};

enum {
    GLOSSY_UNKNOWN_N_TX_MAX = 0
};

enum {
    GLOSSY_UNKNOWN_PAYLOAD_LEN = 0
};

typedef enum {
    GLOSSY_UNKNOWN_SYNC = 0x00,
    GLOSSY_WITH_SYNC = 0x10,
    GLOSSY_WITHOUT_SYNC = 0x20,
    GLOSSY_ONLY_RELAY_CNT = 0x30
} glossy_sync_t;

typedef enum {
    GLOSSY_UNKNOWN_VERSION     = 0x00,
    GLOSSY_STANDARD_VERSION    = 0x40,
    GLOSSY_TX_ONLY_VERSION     = 0x80
    // reserved 0xC0
} glossy_version_t;

/**
 * Return status of Glossy API
 */
typedef enum {
    GLOSSY_STATUS_FAIL = 0,
    GLOSSY_STATUS_SUCCESS = 1,
} glossy_status_t;

/**
 * List of possible Glossy states.
 */
typedef enum {
    GLOSSY_STATE_OFF,
    GLOSSY_STATE_ACTIVE
} glossy_state_t;

#if GLOSSY_STATS
/** Structure defining statistics collected from the
 * moment Glossy was initialised (with glossy_init).
 */
typedef struct {
    // defined by me
    uint8_t  relay_cnt_first_rx;
    // rx_err events
    uint16_t n_rx_err;              /**< Total number of rx errors of any type (SFD timeouts included) */
    uint16_t n_phr_err;             /**< PHR errors */
    uint16_t n_sfd_to;              /**< SFD timeouts */
    uint16_t n_rs_err;              /**< Errors in Reed Solomon decoding phase */
    uint16_t n_fcs_err;             /**< FCS (CRC) errors */
    uint16_t ff_rejects;            /**< Rejections due to frame filtering */
    // rx_timeout events
    uint16_t n_rfw_to;              /**< Received Frame Wait Timeout counter */
    uint16_t n_preamble_to;         /**< Preamble Detection Timeout counter */
    // legacy - checked
    uint16_t rx_timeouts;           /**< Total number of rx timeouts */
    uint16_t n_rx;                  /**< Total number of correct rx */
    uint16_t n_tx;                  /**< Total number of correct tx */
    uint16_t n_bad_length;          /**< Number of received packets with wrong length.
                                        //TODO: check out this in doxygen
                                        pkt_length > glossy_payload_length*/
    uint16_t n_bad_header;          /**< Number of wrong Glossy headers received */
    uint16_t n_payload_mismatch;    /**< Number of payload mismatches obtained from packet rx */
    uint16_t n_length_mismatch;     /**< Number of length mismatches obtained from packet rx */
} glossy_stats_t;
#endif /* GLOSSY_STATS */

//extern volatile uint16_t node_id;

/**
 * \brief  Initialize Glossy internals
 * \return GLOSSY_STATUS_SUCCESS if the initialization is successful. Otherwise GLOSSY_STATUS_FAIL.
 */
glossy_status_t glossy_init(void);


/**
 * \brief       start Glossy
 * \param   initiator_id node ID of the initiator, use
 *              GLOSSY_UNKNOWN_INITIATOR if the initiator is unknown
 * \param payload           pointer to the app payload
 * \param payload_len       length of the app payload, must not exceed the maximum
 * \param n_tx_max          maximum number of retransmissions
 * \param sync              synchronization mode
 * \param start_at_dtu_time if true, start communication at a given radio timestamp
 * \param start_time_dtu)   radio timestamp used to start radio operations
 *
 * start Glossy, i.e. initiate a flood (if node is initiator) or switch to RX
 * mode (receive/relay packets)
 *
 * \note        n_tx_max must be at most 15!
 *
 * \note
 * \p payload is a reference to the external structure representing
 * the payload. When receiving, this structure will hold the
 * received data.
 *
 * \note if start_at_dtu_time is false, the flood will start after
 * GLOSSY_START_DELAY_US after calling glossy_start(). The receivers will
 * start listening immediately.
 *
 * \note if start_at_dtu_time is true, the flood will start at the specified
 * time, meaning that the SFD will exit the antenna of the initiator at
 * start_time_dtu. For receivers, the reception mode will be enabled exactly at
 * start_time_dtu, therefore the application must ensure that enough time
 * is given to allow the radio to turn to RX mode and receive the packet
 * preamble.
 *
 */
glossy_status_t glossy_start(const uint16_t initiator_id,
                             uint8_t* payload,
                             const uint8_t  payload_len,
                             const uint8_t  n_tx_max,
                             const glossy_sync_t sync,
                             const bool start_at_dtu_time,
                             const uint32_t start_time_dtu);

/**
 * \brief            Stop Glossy and resume all other application tasks.
 * \return           Number of times the packet has been received during
 *                   last Glossy phase.
 *                   If it is zero, the packet was not successfully received.
 * \sa               get_rx_cnt
 */
uint8_t glossy_stop(void);

/**
 * \brief  Query activity of glossy
 * \return The number of received bytes since glossy_start was called
 */
uint8_t glossy_is_active(void);

/**
 * \brief Get the number of received packets during the last flood
 * \return Number of receptions during the last flood.
 */
uint8_t glossy_get_n_rx(void);

/**
 * \brief  Get the number of transmitted packets during the last flood
 * \return Number of transmissions during the last flood.
 */
uint8_t glossy_get_n_tx(void);

/**
 * \brief  Get the length of the payload of the received/transmitted packet
 * \return Size of the payload associated with last flood.
 */
uint8_t glossy_get_payload_len(void);

/**
 * \brief  Indicates if the reference time has been updated in the last flood
 * \return non-zero if reference time has been updated. Otherwise zero.
 *
 */
uint8_t glossy_is_t_ref_updated(void);

/**
 * \brief  Get the reference (SFD) time corresponding to the first transmission of the initiator
 * \return 64-bit timestamp (type rtimer_clock_t)
 */
rtimer_clock_t glossy_get_t_ref(void);

/**
 * \brief  Get the reference (SFD) time corresponding to the first transmission of the initiator
 * \return 32-bit timestamp expressed in device time units (dtu).
 */
uint32_t glossy_get_t_ref_dtu(void);

/**
 * \brief  Get the ID of the initiator
 * \return the ID of the initiator which started the last flood.
 */
uint16_t glossy_get_initiator_id(void);

/**
 * \brief  Get synchronization option of last Glossy flood.
 * \return the synchronization option of last Glossy flood.
 */
glossy_sync_t glossy_get_sync_opt(void);

/**
 * \brief Get maximum length of payload
 * \return The maximum length of payload
 */
uint8_t glossy_get_max_payload_len();

/** Return the slot duration based on the expected payload len.*/
uint32_t glossy_get_slot_duration(uint8_t payload_len);
uint32_t glossy_get_round_duration();


#if GLOSSY_STATS
/**
 * \brief Get Glossy statistics
 * \param stats A pointer to statistics structure.
 */
void glossy_get_stats(glossy_stats_t* stats);

/** \brief Print Glossy statistics.
 */
void glossy_stats_print();

/**
 * \brief Get reference relay count
 * \return the relay count of either the first reception or the
 *  first transmission in the last flood.
 */
uint8_t glossy_get_relay_cnt_first_rx(void);

#endif /* GLOSSY_STATS */

/**
 * \brief Print Glossy debug information.
 */
void glossy_debug_print(void);

void glossy_version_print(void);

float glossy_get_ppm_offset();

uint32_t glossy_get_status_reg(void);

#endif /* GLOSSY_H_ */

/** \} */

