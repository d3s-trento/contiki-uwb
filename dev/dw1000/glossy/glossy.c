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
#include "glossy.h"
#include "deca_device_api.h"
#include "deca_regs.h"
/*---------------------------------------------------------------------------*/
#include "dw1000-config.h"
#include "dw1000-util.h"
#include "dw1000-arch.h"
#include "dw1000.h"
#include "spix.h" // XXX platform-specific
/*---------------------------------------------------------------------------*/
#include "sys/node-id.h" // for node_id
#include "net/mac/frame802154.h"
/*---------------------------------------------------------------------------*/
#include "sys/rtimer.h"
/*---------------------------------------------------------------------------*/
#include <string.h>
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

/*
 * Note about the time units used in this file
 *
 *  - "_uus" suffix means the time is in UWB microseconds (512/499.2 microseconds)
 *  - no suffix or "_4ns" suffix means the time is in 4.0064102564 nanosecond
 *    units (the highest 32 bits of DW1000 timestamps)
 *  - "_dtu" means device time unit and is the same as "_4ns"
 *
 */

/*---------------------------------------------------------------------------*/

#define DEBUG           1
#if DEBUG

#include <stdio.h>
#include "leds.h"
#define PRINTF(...)     printf(__VA_ARGS__)
#define LOG(label, ...) PRINTF("["label"]"__VA_ARGS__)

#else

#define GLOSSY_LOG_LEVEL       GLOSSY_LOG_NONE_LEVEL
#define PRINTF(...)     do {} while(0)
#define LOG(...)        PRINTF()

#endif
/*---------------------------------------------------------------------------*/
/*                           LOGGING MACROS                                  */
/*---------------------------------------------------------------------------*/
#define GLOSSY_LOG_INFO_LEVEL           3
#define GLOSSY_LOG_DEBUG_LEVEL          2
#define GLOSSY_LOG_ERROR_LEVEL          1
// convenience levels
#define GLOSSY_LOG_ALL_LEVELS           3
#define GLOSSY_LOG_NONE_LEVEL           0
/*---------------------------------------------------------------------------*/
#if DEBUG
#ifdef GLOSSY_LOG_LEVEL_CONF
#define GLOSSY_LOG_LEVEL                GLOSSY_LOG_LEVEL_CONF
#else
#define GLOSSY_LOG_LEVEL                GLOSSY_LOG_ALL_LEVELS
#endif /* GLOSSY_LOG_LEVEL_CONF */
#endif /* DEBUG */
/*---------------------------------------------------------------------------*/
#if GLOSSY_LOG_INFO_LEVEL <= GLOSSY_LOG_LEVEL
    #define LOG_INFO(...)   LOG("GLOSSY_INFO", __VA_ARGS__)
#else
    #define LOG_INFO(...)   do {} while(0)
#endif
#if GLOSSY_LOG_DEBUG_LEVEL <= GLOSSY_LOG_LEVEL
    #define LOG_DEBUG(...)  LOG("GLOSSY_DEBUG", __VA_ARGS__)
#else
    #define LOG_DEBUG(...)   do {} while(0)
#endif
#if GLOSSY_LOG_ERROR_LEVEL <= GLOSSY_LOG_LEVEL
    #define LOG_ERROR(...)  LOG("GLOSSY_ERROR", __VA_ARGS__)
#else
    #define LOG_ERROR(...)   do {} while(0)
#endif
/*---------------------------------------------------------------------------*/
/*                          GLOSSY CONFIGURATION                             */
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_VERSION_CONF:   GLOSSY_TX_ONLY_VERSION  |
 *                                 GLOSSY_STANDARD_VERSION
 */
#ifdef GLOSSY_VERSION_CONF
#define GLOSSY_VERSION GLOSSY_VERSION_CONF
#else
#define GLOSSY_VERSION GLOSSY_TX_ONLY_VERSION       // default Glossy version
#endif /* GLOSSY_VERSION_CONF */
/*---------------------------------------------------------------------------*/
/* If set to 1 the slot is dynamically estimated based on Rx-Tx and
 * Tx-Rx pairs.
 *
 * If set to 0, the slot is set based on the estimated transmission time
 * of the first frame received.
 *
 * GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF    0 |
 *                                      1
 */
#ifdef GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF
#define GLOSSY_DYNAMIC_SLOT_ESTIMATE GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF
#else
#define GLOSSY_DYNAMIC_SLOT_ESTIMATE    0                       // default slot estimation behaviour
#endif /* GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF */


#ifdef GLOSSY_RX_OPT_CONF
#define GLOSSY_RX_OPT GLOSSY_RX_OPT_CONF
#else
#define GLOSSY_RX_OPT 1
#endif


#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#define STATETIME_MONITOR(...) __VA_ARGS__
#else
#define STATETIME_MONITOR(...) do {} while(0)
#endif

/*---------------------------------------------------------------------------*/
/*                           GLOSSY PACKET                                   */
/*---------------------------------------------------------------------------*/
#define GLOSSY_USE_802154_FRAME         0
#if GLOSSY_USE_802154_FRAME
#define IEEE_HDR_LEN                    3   // IEEE headers are 3B in our case:
                                            // Frame Control field (2B) + SeqNo (1B - mandatory)
#else
#define IEEE_HDR_LEN                    0   // no IEEE header
#endif
#define GLOSSY_MAX_PSDU_LEN             (DW1000_MAX_PACKET_LEN)
#define GLOSSY_MIN_PSDU_LEN             (IEEE_HDR_LEN + sizeof(glossy_header_t) + DW1000_CRC_LEN)
#define GLOSSY_PAYLOAD_OFFSET           (IEEE_HDR_LEN + sizeof(glossy_header_t))
#define GLOSSY_PAYLOAD_LEN(psdu_len)    ((psdu_len) - IEEE_HDR_LEN - sizeof(glossy_header_t) - DW1000_CRC_LEN)
#define GLOSSY_MAX_PAYLOAD_LEN          GLOSSY_PAYLOAD_LEN(GLOSSY_MAX_PSDU_LEN)

#define GLOSSY_PSDU_NONE                0   // means that no packet was received or sent

#define GLOSSY_MAX_N_TX                 255
#define GLOSSY_CONFIG_SYN_MASK          0x30U
#define GLOSSY_CONFIG_VER_MASK          0xc0U
/*---------------------------------------------------------------------------*/
#define GET_BITMASK(VALUE, MASK) \
    (VALUE & MASK)
#define APPLY_BITMASK(VALUE, MASK, BIT_VALUE) \
    VALUE = (VALUE & ~MASK) | (BIT_VALUE & MASK)
/*---------------------------------------------------------------------------*/
#define GLOSSY_SET_VERSION(CONFIG_FIELD, BIT_VALUE)\
    APPLY_BITMASK(CONFIG_FIELD, GLOSSY_CONFIG_VER_MASK, BIT_VALUE)

#define GLOSSY_GET_VERSION(CONFIG_FIELD)\
    GET_BITMASK(CONFIG_FIELD, GLOSSY_CONFIG_VER_MASK)
/*---------------------------------------------------------------------------*/
#define GLOSSY_SET_SYNC(CONFIG_FIELD, BIT_VALUE)\
    APPLY_BITMASK(CONFIG_FIELD, GLOSSY_CONFIG_SYN_MASK, BIT_VALUE)

#define GLOSSY_GET_SYNC(CONFIG_FIELD)\
    GET_BITMASK(CONFIG_FIELD, GLOSSY_CONFIG_SYN_MASK)
/*---------------------------------------------------------------------------*/
/** \struct glossy_header_t
 *  The structure defining the header of a Glossy packet.
 *
 *  Configuration Field (config):
 *
 *  +-----+-----+----------+
 *  | VER | SYN | RESERVED |
 *  +-----+-----+----------+
 *  7    6 5   4 3         0
 *
 *  VERSION (VER):
 *  - 0: Standard Glossy, Transmission occurs right after packet reception.
 *  - 1: Repeated TX, once received the first packet, repeat transmission
 *       scheduled every T_slot time.
 *
 *  SYNC Packet (SYN):
 *  - 0: The packet is not used in the slot estimation algorithm.
 *  - 1: The packet is used in the slot estimation algorithm.
 */
typedef struct glossy_header_t {
    uint16_t    initiator_id;           // this makes the struct
    uint8_t     config;                 // 2 byte-aligned, total size = 6 Bytes
    uint8_t     relay_cnt;
    uint8_t     max_n_tx;
} glossy_header_t;
typedef uint8_t glossy_payload_t;
/*---------------------------------------------------------------------------*/
/*                           GLOSSY CONTEXT INFO                             */
/*---------------------------------------------------------------------------*/
/** \struct glossy_context_t
 *  The context containing information for a single Glossy call.
 */
typedef struct glossy_context_t {

    // the header for the current packet (rx and/or tx)
    glossy_header_t pkt_header;
    // a pointer to the external buffer where the payload will be stored
    glossy_payload_t *pkt_payload;
    // phy payload length or GLOSSY_PSDU_NONE if unknown (neither received
    // nor transmitted a packet)
    uint16_t psdu_len;
    /*-----------------------------------------------------------------------*/
    // the timestamp of the SFD of the first packet rx
    uint32_t tref;
    // the relay cnt of the first packet rx or tx
    uint8_t  tref_relay_cnt;
    bool     tref_updated;
    /*-----------------------------------------------------------------------*/
    // starting time of last tx and rx
    uint32_t ts_last_rx;
    uint32_t ts_last_tx;
    uint8_t  relay_cnt_last_rx;
    uint8_t  relay_cnt_last_tx;
    /*-----------------------------------------------------------------------*/
    uint8_t n_tx;
    // note: n_rx is the total number of rx
    // (comprising rx of the pkt with the same relay counter)
    uint8_t n_rx;
    /*-----------------------------------------------------------------------*/
    // slot variables
    uint32_t slot_sum;
    uint8_t  n_slots;
    uint32_t slot_duration;
    // activation/deactivation timestamps (only used for logging)
    uint32_t ts_start;
    uint32_t ts_stop;
    /*-----------------------------------------------------------------------*/
    // DW1000 status
    uint32_t status_reg;
    /*-----------------------------------------------------------------------*/
    glossy_state_t state;
    /*-----------------------------------------------------------------------*/
    #if GLOSSY_STATS
    glossy_stats_t stats;
    #endif
    float ppm_offset;
} glossy_context_t;
/*---------------------------------------------------------------------------*/
/*                           UTILITY FUNCTIONS DEFINITION                    */
/*---------------------------------------------------------------------------*/

// With the standard Crystal implementation that schedules T slots using
// RTimer we should add a slack to our guard times before and after the expected
// reception time
//#define GLOSSY_LOOSEN_GUARDS_UUS 32   // add a RTimer tick duration to guards
#define GLOSSY_LOOSEN_GUARDS_UUS 0  // don't add anything (normal operation)

/** \def Define the guard time added to the rx timeout.
 */
#define GLOSSY_RX_TIMEOUT_GUARD_UUS     (10 + GLOSSY_LOOSEN_GUARDS_UUS)

/** \def Define the guard time when glossy is optizimed for just-in-time rx.
 */
#define GLOSSY_RX_OPT_GUARD_UUS         (10 + GLOSSY_LOOSEN_GUARDS_UUS)


/*---------------------------------------------------------------------------*/
/*                           OUTPUT CONFIGURATION MACROS                     */
/*---------------------------------------------------------------------------*/
#if UWB_CONTIKI_PRINT_DEF
#include "print-def.h"
#pragma message STRDEF(GLOSSY_VERSION)
#pragma message STRDEF(GLOSSY_DYNAMIC_SLOT_ESTIMATE)
#pragma message STRDEF(GLOSSY_RX_OPT)
#pragma message STRDEF(GLOSSY_LOG_LEVEL)
#pragma message STRDEF(GLOSSY_RX_OPT_GUARD_UUS)
#endif


/*---------------------------------------------------------------------------*/
/**
 * \def Convert a ~4ns accurate time to a 1 UWB microsecond accurate time.
 *
 * Conversion assumes a ~4ns accurate time is given, taken by the 32 higher
 * order bits of the device's timestamp. To obtain the corresponding time
 * in UWB microseconds, just drop the 8 lower level bits of the 32 bit time value.
 *
 * from 4ns (2^8) to 1us (2^16)
 */
#define GLOSSY_DTU_4NS_TO_UUS(NS4_ACCURATE_TIME)    ((NS4_ACCURATE_TIME) >> 8)

/** \def Convert a 15.65ps accurate timedelta to a 4ns accurate timedelta.
 *
 * The conversion assues a ~15.65ps accurate time is given. To obtain the
 * corresponding time in UWB nanoseconds, just drop the 8 lower level bits.
 *
 * from 15.65ps (2^0) to ~4ns (2^8)
 */
#define GLOSSY_DTU_15PS_TO_4NS(PS15_ACCURATE_TIME)  ((PS15_ACCURATE_TIME) >> 8)
/*---------------------------------------------------------------------------*/
static void glossy_context_init();
/*---------------------------------------------------------------------------*/
#if GLOSSY_STATS
/** \brief Initialised statistics counters.
 *
 * Initialise counters used when collecting statistics on the
 * last Glossy flood.
 */
static void glossy_stats_init();
#endif /* GLOSSY_STATS */
/*---------------------------------------------------------------------------*/
/** \brief Update the reference time and reference relay counter.
 */
static inline void tref_update(const uint32_t tref, const uint8_t tref_relay_cnt);
/*---------------------------------------------------------------------------*/
#if GLOSSY_DYNAMIC_SLOT_ESTIMATE
/** \brief Add slot to the context slot sum and increment the slot counter.
 */
static inline void add_slot(const uint32_t slot_duration);
#endif /* GLOSSY_DYNAMIC_SLOT_ESTIMATE */
/*---------------------------------------------------------------------------*/
/** \brief Estimate the slot based on the payload length.
 *
 *  The length considered is given by glossy_header + payload + crc.
 */
static uint32_t calc_slot_duration(uint16_t psdu_len);
/*---------------------------------------------------------------------------*/
static inline uint32_t glossy_get_rx_delay_uus(uint16_t psdu_len);
/*---------------------------------------------------------------------------*/
static inline uint16_t glossy_get_rx_timeout_uus(uint16_t psdu_len);
/*---------------------------------------------------------------------------*/
/** \brief Convert from dw1000 timer to rtimer_clock_t.
 */
static rtimer_clock_t radio_to_rtimer_ts(uint32_t radio_ts);
/*---------------------------------------------------------------------------*/
static inline bool is_glossy_initiator();
/*---------------------------------------------------------------------------*/
/** \brief Resume the flood at the initiator after an erroneous reception
 */
static inline int glossy_resume_flood();
/*---------------------------------------------------------------------------*/
/** \brief Validate the received Glossy header against the one stored in
 *  the context.
 */
static inline glossy_status_t glossy_validate_header(const glossy_header_t* rcvd_header);
/*---------------------------------------------------------------------------*/
/*                           GLOSSY FRAME HANDLING                           */
/*---------------------------------------------------------------------------*/
/** \brief Frame control field for a 802.15.4 frame.
 */
typedef struct frame_control_field_t {
    uint8_t frame_type;                     // 3bits
    uint8_t security_enabled;               // 1bit
    uint8_t frame_pending;                  // 1bit
    uint8_t ack_required;                   // 1bit
    uint8_t panid_compression;              // 1bit
    uint8_t reserved;                       // 1bit
    uint8_t seqno_suppression;              // 1bit
    uint8_t ie_present;                     // 1bit -> not supported
    uint8_t dest_addr_mode;                 // 2bits
    uint8_t frame_version;                  // 2bits
    uint8_t src_addr_mode;                  // 2bits
} frame_control_field_t;

#if GLOSSY_USE_802154_FRAME
// Default frame control field used by Glossy packet.
// The only overhead will be given by the fcf field (2B).
static frame_control_field_t glossy_fcf = {
    .frame_type         = 0x11,
    .security_enabled   = 0x0,
    .frame_pending      = 0x0,
    .ack_required       = 0x0,
    .panid_compression  = 0x0,
    .seqno_suppression  = 0x0, // not supported -> cannot be set to 1
    //.ie_list_present  = 0x0, // not supported
    .dest_addr_mode     = 0x0,
    .frame_version      = 0x0,  // 802.15.4-2003 version
                                // The DW1000 frame filtering functionality will
                                // filter any version but 01 and 00 (so no
                                // sequence number suppression is available)
    .src_addr_mode      = 0x0
};
#endif /* GLOSSY_USE_802154_FRAME */
/*---------------------------------------------------------------------------*/
/** \brief Generate a Glossy frame stored to the given buffer.
 *
 * \param glossy_header The header for the Glossy packet.
 * \param payload       The Glossy payload.
 * \param payload_len   The length of the Glossy payload.
 * \param buffer        An external buffer where the prepared packet has
 *                      to be stored.
 *
 * \return PSDU length of the created frame
 */
static int
glossy_frame_new(const glossy_header_t *header,
        const glossy_payload_t *payload,
        const uint8_t payload_len,
        uint8_t *buffer)
{
    size_t offset = 0;
    #if GLOSSY_USE_802154_FRAME
    buffer[0] =
        (glossy_fcf.frame_type & 7) |
        ((glossy_fcf.security_enabled & 1)  << 3) |
        ((glossy_fcf.frame_pending & 1)     << 4) |
        ((glossy_fcf.ack_required & 1)      << 5) |
        ((glossy_fcf.panid_compression & 1) << 6);

    buffer[1] =
        (glossy_fcf.seqno_suppression & 1) |
        ((glossy_fcf.dest_addr_mode & 3)    << 2) |
        ((glossy_fcf.frame_version & 3)     << 4) |
        ((glossy_fcf.src_addr_mode & 3)     << 6);
    buffer[2] = 0x0;                                // 0 as seq number
    offset = IEEE_HDR_LEN;
    #endif /* GLOSSY_USE_802154_FRAME */

    // attach MAC payload:
    // copy the glossy_header and payload to the buffer
    memcpy(buffer + offset, header, sizeof(glossy_header_t));
    offset += sizeof(glossy_header_t);
    memcpy(buffer + offset, payload, sizeof(uint8_t) * payload_len);
    offset += sizeof(uint8_t) * payload_len;
    return offset + DW1000_CRC_LEN;
}
/*---------------------------------------------------------------------------*/
/** Update the glossy header in the given buffer
 */
static inline void
glossy_update_hdr(const glossy_header_t* g_header, uint8_t *buffer)
{
    memcpy(buffer + IEEE_HDR_LEN, g_header, sizeof(glossy_header_t));
}
/*---------------------------------------------------------------------------*/
/*                          STATIC VARIABLES                                 */
/*---------------------------------------------------------------------------*/

// Packet buffer for storing a correct packet:
//  - initiator: the last one transmitted
//  - receiver:  the first one received and verified or the last one
//               transmitted (if any)
static uint8_t clean_buffer[GLOSSY_MAX_PSDU_LEN];

// Packet buffer for reading unverified received data
static uint8_t dirty_buffer[GLOSSY_MAX_PSDU_LEN];

static glossy_context_t g_context;
static bool glossy_initialised = false;   // set to true upon glossy_init
static uint32_t tx_antenna_delay_4ns;     // cache the antenna delay value
/*---------------------------------------------------------------------------*/
/*                           DW1000 CALLBACK FUNCTIONS                       */
/*---------------------------------------------------------------------------*/
static void glossy_tx_done_cb(const dwt_cb_data_t *cbdata);
static void glossy_rx_ok_cb(const dwt_cb_data_t *cbdata);
static void glossy_rx_to_cb(const dwt_cb_data_t *cbdata);
static void glossy_rx_err_cb(const dwt_cb_data_t *cbdata);
/*---------------------------------------------------------------------------*/
/*                           DW1000 CALLBACK IMPLEMENTATION                  */
/*---------------------------------------------------------------------------*/
static void
glossy_tx_done_cb(const dwt_cb_data_t *cbdata)
{
    /* NOTE:
     * don't go to rx state (explicitily) here. Instead use the appropriate
     * driver function to switch to rx right after finishing frame
     * transmission.
     *
     * NOTE: do NOT issue dwt_forcetrx if you are not explicitly TX.
     * RX after TX could be in place and would be interrupted otherwise!
     */
    uint32_t tx_time = dwt_readtxtimestamphi32();
    uint32_t ts_tx_4ns = 0;
    int status;                         // hold intermediate radio functions' return value
    STATETIME_MONITOR(dw1000_statetime_after_tx(tx_time, g_context.psdu_len));
    /*-----------------------------------------------------------------------*/
    // GLOSSY SLOT ESTIMATION
    /*-----------------------------------------------------------------------*/
    g_context.ts_last_tx = tx_time;
    g_context.relay_cnt_last_tx = g_context.pkt_header.relay_cnt;

    if (GLOSSY_GET_SYNC(g_context.pkt_header.config) == GLOSSY_WITH_SYNC) {

        if (!g_context.tref_updated) {
            tref_update(g_context.ts_last_tx, g_context.pkt_header.relay_cnt);
        }

        #if GLOSSY_DYNAMIC_SLOT_ESTIMATE
        if (g_context.relay_cnt_last_tx == g_context.relay_cnt_last_rx + 1 &&
                g_context.n_rx > 0) {
            // slot estimation
            add_slot(g_context.ts_last_tx - g_context.ts_last_rx);
        }
        #endif /* GLOSSY_DYNAMIC_SLOT_ESTIMATE */

    }
    // increment tx counter
    g_context.n_tx += 1;
    // GLOSSY SLOT ESTIMATION - END
    /*-----------------------------------------------------------------------*/
    // GLOSSY_TX_ONLY_VERSION
    /*-----------------------------------------------------------------------*/
    if (g_context.n_tx  >= g_context.pkt_header.max_n_tx) {
        glossy_stop();
    } else if (GLOSSY_GET_VERSION(g_context.pkt_header.config) ==
               GLOSSY_TX_ONLY_VERSION) {

        ts_tx_4ns = tx_time + g_context.slot_duration - tx_antenna_delay_4ns;
        // update the relay counter, prepare the new packet and TX later
        g_context.pkt_header.relay_cnt += 1;
        glossy_update_hdr(&g_context.pkt_header, clean_buffer);

        dwt_writetxdata(g_context.psdu_len, clean_buffer, 0);
        dwt_writetxfctrl(g_context.psdu_len, 0, 0);

        //dwt_forcetrxoff();  // we can avoid this since the radio goes automatically to IDLE after TX
        dwt_setdelayedtrxtime(ts_tx_4ns);
        // TX and do not go to rx state after transmitting
        /* errata TX-1: ensure TX done is issued */
        dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
        status = dwt_starttx(DWT_START_TX_DELAYED);

        if (status != DWT_SUCCESS) {
            LOG_ERROR("Tx cb: Failed to TX\n");
            return;
        }
        else {
            STATETIME_MONITOR(dw1000_statetime_schedule_tx(ts_tx_4ns));
        }
    }
    /*-----------------------------------------------------------------------*/
    // DEBUG FEEDBACK
    /*-----------------------------------------------------------------------*/
    leds_toggle(LEDS_YELLOW);
    LOG_DEBUG("TX succeeded at: %"PRIu32"\n", tx_time);
}
/*---------------------------------------------------------------------------*/
static void
glossy_rx_ok_cb(const dwt_cb_data_t *cbdata)
{
    glossy_header_t rcvd_header;
    uint32_t ts_tx_4ns;
    int status;                    // hold intermediate radio functions' return value
    int frame_error;               // error while parsing the received frame

    /*-----------------------------------------------------------------------*/
    // TODO:REFACTOR: force IDLE state
    // dwt_forcetrxoff(); // XXX IMO no need to trxoff here.
    uint32_t ts_rx_4ns = dwt_readrxtimestamphi32();
    STATETIME_MONITOR(dw1000_statetime_after_rx(ts_rx_4ns, cbdata->datalength));
    /*-----------------------------------------------------------------------*/

    frame_error = 0;

    /*-----------------------------------------------------------------------*/
    /* NOTE:
     * we receive only Glossy packets, everything not
     * compliant with their characteristics has to be considered
     * garbage.
     */
    // BAD_LENGTH: the packet is smaller than the Glossy header or
    // the payload is too long
    if (cbdata->datalength < GLOSSY_MIN_PSDU_LEN || cbdata->datalength > GLOSSY_MAX_PSDU_LEN) {
        LOG_DEBUG("Invalid Glossy packet size. Packet ignored\n");
        #if GLOSSY_STATS
        g_context.stats.n_bad_length++;
        #endif /* GLOSSY_STATS */
        frame_error = 1;
    }

    /* Read the frequency offset w.r.t. the transmitter (only for debugging) */
    g_context.ppm_offset = dw1000_get_ppm_offset(dw1000_get_current_cfg()); // TODO: make optional?

    if (!frame_error) {
        // if it is the first packet we see, use the clean buffer directly
        uint8_t *dest_buffer = (g_context.psdu_len == GLOSSY_PSDU_NONE) ? clean_buffer : dirty_buffer;
        /*-----------------------------------------------------------------------*/
        // read pkt from transceiver to local buffer
        dwt_readrxdata(dest_buffer, cbdata->datalength - DW1000_CRC_LEN, 0);
        memcpy(&rcvd_header, dest_buffer + IEEE_HDR_LEN, sizeof(glossy_header_t)); // retrieve glossy header
        /*-------------------------------------------------------------------*/
        // check header and payload
        if (glossy_validate_header(&rcvd_header) != GLOSSY_STATUS_SUCCESS) {
            LOG_DEBUG("Invalid header received. Packet ignored\n");
            #if GLOSSY_STATS
            g_context.stats.n_bad_header++;
            #endif /* GLOSSY_STATS */
            frame_error = 1;
        }
    }
    // check if the received length matches the one we already have (if we do)
    if (!frame_error && g_context.psdu_len != GLOSSY_PSDU_NONE && g_context.psdu_len != cbdata->datalength) {
        LOG_DEBUG("Mismatching length received. Packet ignored\n");
        #if GLOSSY_STATS
        g_context.stats.n_length_mismatch++;
        #endif /* GLOSSY_STATS */
        frame_error = 1;
    }
    // check if the received payload matches the one we already have (if we do)
    if (!frame_error && g_context.psdu_len != GLOSSY_PSDU_NONE) {
        if (memcmp(dirty_buffer + GLOSSY_PAYLOAD_OFFSET,
                   clean_buffer + GLOSSY_PAYLOAD_OFFSET,
                   GLOSSY_PAYLOAD_LEN(cbdata->datalength)) != 0) {
            // payloads do not match, reject
            LOG_DEBUG("Mismatching payload received. Packet ignored\n");
            #if GLOSSY_STATS
            g_context.stats.n_payload_mismatch++;
            #endif /* GLOSSY_STATS */
            frame_error = 1;
        }
    }

    if (frame_error) {
        if (is_glossy_initiator()) {
            // Initiator retransmits the packet in its next tx slot
            // (this only happens with the standard version)
            status = glossy_resume_flood();

            if (status != DWT_SUCCESS) {
                // if performing a delayed transmission probably the transceiver
                // failed to tx within the deadline
                LOG_ERROR("Rx cb1: Failed to TX\n");
            }
        }
        else {
            // non-initiators continue listening
            dwt_setrxtimeout(0);
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
            STATETIME_MONITOR(dw1000_statetime_schedule_rx(dwt_readsystimestamphi32()));
        }
        return; // stop processing the erroneous frame
    }

    /* At this point all checks have passed */

    // store the pkt header and length to the context
    g_context.pkt_header  = rcvd_header;
    g_context.psdu_len    = cbdata->datalength;

    // update context pkt relay counter and the packet header
    g_context.pkt_header.relay_cnt += 1;
    glossy_update_hdr(&g_context.pkt_header, clean_buffer);

    // write the frame data to the radio
    dwt_writetxdata(g_context.psdu_len, clean_buffer, 0);
    dwt_writetxfctrl(g_context.psdu_len, 0, 0);

    #if GLOSSY_STATS
    // save the relay counter of the first glossy packet correctly rx
    if (g_context.n_rx == 0) {
        g_context.stats.relay_cnt_first_rx = rcvd_header.relay_cnt;
    }
    #endif /* GLOSSY_STATS */

    /* SLOT ESTIMATION ALGORITHM --------------------------------------------*/
    g_context.ts_last_rx = ts_rx_4ns;
    // increment rx counter
    g_context.n_rx += 1;
    g_context.relay_cnt_last_rx = rcvd_header.relay_cnt;

    if (GLOSSY_GET_SYNC(g_context.pkt_header.config) == GLOSSY_WITH_SYNC) {
        if (!g_context.tref_updated) {
            tref_update(g_context.ts_last_rx, rcvd_header.relay_cnt);
        }
        #if GLOSSY_DYNAMIC_SLOT_ESTIMATE
        if (g_context.relay_cnt_last_rx == g_context.relay_cnt_last_tx + 1 &&
                g_context.n_tx > 0) {
            add_slot(g_context.ts_last_rx - g_context.ts_last_tx);
        }
        #endif /* GLOSSY_DYNAMIC_SLOT_ESTIMATE */
    }
    /* END OF SLOT ESTIMATION ALGORITHM -------------------------------------*/

    // calculate slot duration based on packet length
    g_context.slot_duration = calc_slot_duration(g_context.psdu_len);
    ts_tx_4ns = ts_rx_4ns + g_context.slot_duration - tx_antenna_delay_4ns;

    /*-----------------------------------------------------------------------*/

    // stop Glossy if max relay reached
    if (g_context.n_tx >= g_context.pkt_header.max_n_tx) {
        glossy_stop();
    } else {
        glossy_version_t ver = 0x0;
        uint32_t rx_delay_uus = 0;
        // schedule TX.
        // Note: the radio is currently in idle state
        if (GLOSSY_GET_VERSION(g_context.pkt_header.config) ==
                GLOSSY_TX_ONLY_VERSION) {
            // We received a packet and now schedule our first
            // retransmission, **without** expecting a response!
            dwt_setdelayedtrxtime(ts_tx_4ns);
            /* errata TX-1: ensure TX done is issued */
            dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
            status = dwt_starttx(DWT_START_TX_DELAYED);
            ver = GLOSSY_TX_ONLY_VERSION;

        } else if (GLOSSY_GET_VERSION(g_context.pkt_header.config) ==
                GLOSSY_STANDARD_VERSION) {

            // if RX optimisation is enabled, set the RX after TX delay to a non-zero value
            rx_delay_uus = glossy_get_rx_delay_uus(g_context.psdu_len);
            dwt_setrxaftertxdelay(rx_delay_uus);

            if (is_glossy_initiator()) {
                // set RX timeout so that the initiator can resume the flood
                // even if it misses the RX
                uint16_t rx_timeout_uus = glossy_get_rx_timeout_uus(g_context.psdu_len);
                dwt_setrxtimeout(rx_timeout_uus);
            }
            // start delayed TX and request RX mode after
            dwt_setdelayedtrxtime(ts_tx_4ns);
            /* errata TX-1: ensure TX done is issued */
            dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
            status = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
            ver = GLOSSY_STANDARD_VERSION;

        } else {
            // eventually remove this after checking
            status = DWT_ERROR;
            LOG_ERROR("Invalid version found!");
        }

        if (status != DWT_SUCCESS) {
            // if performing a delayed transmission probably the transceiver
            // failed to tx within the deadline
            LOG_ERROR("Rx cb2: Failed to TX\n");
        }
        else {
            if (ver == GLOSSY_TX_ONLY_VERSION)
                STATETIME_MONITOR(dw1000_statetime_schedule_tx(ts_tx_4ns));
            else
                STATETIME_MONITOR(dw1000_statetime_schedule_txrx(ts_tx_4ns, rx_delay_uus));
        }
    }
    /*-----------------------------------------------------------------------*/
    // DEBUG FEEDBACK
    /*-----------------------------------------------------------------------*/
    leds_toggle(LEDS_YELLOW);
    LOG_DEBUG("RX succeeded at: %"PRIu32"\n", ts_rx_4ns);
}
/*---------------------------------------------------------------------------*/
static void
glossy_rx_to_cb(const dwt_cb_data_t *cbdata)
{
    bool tx_again  = false;
    int status;
    STATETIME_MONITOR(uint32_t now = dwt_readsystimestamphi32(); dw1000_statetime_after_rxerr(now));
    /*-----------------------------------------------------------------------*/
    // collect debugging info first
    #if GLOSSY_STATS
    if (cbdata->status & SYS_STATUS_RXPTO)          {
        g_context.stats.n_preamble_to++;
    } else if (cbdata->status & SYS_STATUS_RXRFTO)  {
        g_context.stats.n_rfw_to++;
    } else {
        LOG_DEBUG("Unkown RX error found. Status: %lx\n", cbdata->status);
    }
    g_context.stats.rx_timeouts++;
    #endif /* GLOSSY_STATS */
    /*-----------------------------------------------------------------------*/

    if (is_glossy_initiator()) {
        // Initiator retransmits the packet in its next TX slot
        // (this only happens with the standard version)
        status = glossy_resume_flood();
        tx_again = true;
    } else {
        // Non-initiators keep listening
        dwt_setrxtimeout(0);
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        STATETIME_MONITOR(dw1000_statetime_schedule_rx(now));
    }
    /*-----------------------------------------------------------------------*/
    // DEBUG FEEDBACK
    /*-----------------------------------------------------------------------*/
    LOG_DEBUG("RX timeout!\n");
    if (tx_again) {
        LOG_DEBUG("Trying to TX again!\n");
        if (status != DWT_SUCCESS) {
            LOG_ERROR("To cb: Failed to TX\n");
        }
    }
    leds_toggle(LEDS_YELLOW);
}
/*---------------------------------------------------------------------------*/
static void
glossy_rx_err_cb(const dwt_cb_data_t *cbdata)
{
    int tx_again = false;
    int status;
    STATETIME_MONITOR(uint32_t now = dwt_readsystimestamphi32(); dw1000_statetime_after_rxerr(now));

    #if GLOSSY_STATS
    // detect the source of the rx error and increment
    // the corresponding counter
    if (cbdata->status & SYS_STATUS_RXPHE)          {
        g_context.stats.n_phr_err++;
    } else if (cbdata->status & SYS_STATUS_RXSFDTO) {
        g_context.stats.n_sfd_to++;
    } else if (cbdata->status & SYS_STATUS_RXRFSL)  {
        g_context.stats.n_rs_err++;
    } else if (cbdata->status & SYS_STATUS_RXFCE)   {
        g_context.stats.n_fcs_err++;
    } else if (cbdata->status & SYS_STATUS_AFFREJ)  {
        g_context.stats.ff_rejects++;
    } else {
        LOG_DEBUG("Unkown RX error found. Status: %lx\n", cbdata->status);
    }
    g_context.stats.n_rx_err++;
    #endif /* GLOSSY_STATS */

    // store the reception error to report it to the application
    g_context.status_reg = g_context.status_reg | cbdata->status;

    if (is_glossy_initiator()) {
        // Initiator retransmits the packet in its next TX slot
        // (this only happens with the standard version)
        status = glossy_resume_flood();
        tx_again = true;
    } else {
        // Non-initiators keep listening
        dwt_setrxtimeout(0);
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        STATETIME_MONITOR(dw1000_statetime_schedule_rx(now));
    }
    /*-----------------------------------------------------------------------*/
    // DEBUG FEEDBACK
    /*-----------------------------------------------------------------------*/
    LOG_DEBUG("RX error!\n");
    if (tx_again) {
        LOG_DEBUG("Trying to TX again!\n");
        if (status != DWT_SUCCESS) {
            LOG_ERROR("Err cb: Failed to TX\n");
        }
    }
    leds_toggle(LEDS_YELLOW);
}
/*---------------------------------------------------------------------------*/
/*                           GLOSSY API IMPLEMENTATION                       */
/*---------------------------------------------------------------------------*/
glossy_status_t
glossy_init(void)
{
    uint16_t rx_ant_dly, tx_ant_dly;
    // Make sure the radio is off
    dwt_forcetrxoff();

    // Set interrupt handlers
    dw1000_set_isr(dwt_isr);
    dwt_setcallbacks(&glossy_tx_done_cb,
            &glossy_rx_ok_cb,
            &glossy_rx_to_cb,
            &glossy_rx_err_cb);
    /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
    dwt_setinterrupt(
            DWT_INT_TFRS  | DWT_INT_RFCG | DWT_INT_RFTO |
            DWT_INT_RXPTO | DWT_INT_RPHE | DWT_INT_RFCE |
            DWT_INT_RFSL  | DWT_INT_SFDT | DWT_INT_ARFE , 1);

    /* Make sure frame filtering is disabled */
    dwt_enableframefilter(DWT_FF_NOTYPE_EN);

    /* Convert the current antenna delay to 4ns for future use */
    dw1000_get_current_ant_dly(&rx_ant_dly, &tx_ant_dly);
    tx_antenna_delay_4ns = GLOSSY_DTU_15PS_TO_4NS(tx_ant_dly);

    if (dw1000_get_current_cfg()->txPreambLength == DWT_PLEN_64) {
        // Use radio settings optimised for preamble length 64
        // XXX (it is unclear how to revert this once set)
        dwt_configurefor64plen(DWT_PRF_64M);

        // further optimisations recommended for preamble length 64
        /* // TODO: check whether this improves anything
        spix_change_speed(DW1000_SPI, DW1000_SPI_SLOW);
        dwt_loadopsettabfromotp(DWT_OPSET_64LEN);
        spix_change_speed(DW1000_SPI, DW1000_SPI_FAST);
        */

    }
    #if GLOSSY_STATS
    glossy_stats_init();
    #endif /* GLOSSY_STATS */
    glossy_initialised = true;
    return GLOSSY_STATUS_SUCCESS;
}
/*---------------------------------------------------------------------------*/
glossy_status_t
glossy_start(const uint16_t initiator_id,
        uint8_t* payload,
        const uint8_t  payload_len,
        const uint8_t  n_tx_max,
        const glossy_sync_t sync,
        const bool start_at_dtu_time,
        const uint32_t start_time_dtu)
{
    glossy_header_t  g_header;
    int status;                   // keep the status of a intermediate operation
    uint32_t call_time_4ns = dwt_readsystimestamphi32();

    dwt_forcetrxoff();

    // if glossy has not been init then return failure
    if (!glossy_initialised) {
        LOG_ERROR("Glossy has to be initialised before issuing start\n");
        return GLOSSY_STATUS_FAIL;
    }

    // init state common to both initiator and receiver
    glossy_context_init();
    STATETIME_MONITOR(dw1000_statetime_context_init(); dw1000_statetime_start(););

    memset(clean_buffer, 0, sizeof(clean_buffer));

    // store the pointer to app payload
    g_context.pkt_payload = payload;

    if (node_id == initiator_id) {
        // the node is the initiator

        // Check input parameters
        if (payload == NULL) {
            LOG_ERROR("Undefined payload given\n");
            return GLOSSY_STATUS_FAIL;
        }
        if (payload_len > GLOSSY_MAX_PAYLOAD_LEN) {
            LOG_ERROR("Provided Glossy payload exceeds maximum"
                    " payload length: %uB\n", GLOSSY_MAX_PAYLOAD_LEN);
            return GLOSSY_STATUS_FAIL;
        }
        if (n_tx_max > GLOSSY_MAX_N_TX) {
            LOG_ERROR("Maximum number of retransmissions provided"
                    " exceeds the maximum value: %u\n",
                    GLOSSY_MAX_N_TX);
            return GLOSSY_STATUS_FAIL;
        }

        // prepare the glossy packet
        g_header.initiator_id  = initiator_id;
        g_header.relay_cnt     = 0;
        g_header.max_n_tx      = n_tx_max;
        g_header.config        = 0x0;
        GLOSSY_SET_SYNC(g_header.config, sync);
        GLOSSY_SET_VERSION(g_header.config, GLOSSY_VERSION);

        // store the current header in the context
        g_context.pkt_header = g_header;

        // every packet field is set, create the packet
        // (header + payload) and load it to the radio buffer
        g_context.psdu_len = glossy_frame_new(&g_header, payload, payload_len, clean_buffer);
        dwt_writetxdata(g_context.psdu_len, clean_buffer, 0);
        dwt_writetxfctrl(g_context.psdu_len, 0, 0);

        // calculate the slot duration based on the packet length
        g_context.slot_duration = calc_slot_duration(g_context.psdu_len);

        // set the reference (SFD) time for the initiator
        if (start_at_dtu_time) {
            g_context.tref = start_time_dtu;
        }
        else {
            g_context.tref = call_time_4ns + GLOSSY_START_DELAY_4NS;
        }

        // mark that the reference time was established
        g_context.tref_updated = true;

        /*-------------------------------------------------------------------*/
        // Set TX and RX delays
        //
        // Some time passes between the call to glossy_start() and the SFD transmission.
        // This time depends on CPU speed, packet length and preamble length.
        // Currently we compensate it for the max packet length and preamble of 128 symbols.
        // TODO: take into consideration the configured current preamble length.

        // calculate the delayed TX time so that SFD exits the antenna exactly at tref
        uint32_t ts_tx_4ns = g_context.tref - tx_antenna_delay_4ns;
        uint32_t rx_delay_uus = 0;
        glossy_version_t ver = 0x0;

        g_context.ts_start = ts_tx_4ns;

        if (GLOSSY_GET_VERSION(g_context.pkt_header.config) ==
                GLOSSY_TX_ONLY_VERSION) {

            // start delayed TX without switching to RX after
            dwt_setdelayedtrxtime(ts_tx_4ns);
            /* errata TX-1: ensure TX done is issued */
            dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
            status = dwt_starttx(DWT_START_TX_DELAYED);
            ver = GLOSSY_TX_ONLY_VERSION;

        } else {
            // set RX timeout so that the initiator can resume the flood
            // even if it misses the RX
            uint16_t rx_timeout_uus = glossy_get_rx_timeout_uus(g_context.psdu_len);
            dwt_setrxtimeout(rx_timeout_uus);

            // if RX optimisation is enabled, set the RX after TX delay to a non-zero value
            rx_delay_uus = glossy_get_rx_delay_uus(g_context.psdu_len);
            dwt_setrxaftertxdelay(rx_delay_uus);

            // start delayed TX and request switching to RX after
            dwt_setdelayedtrxtime(ts_tx_4ns);
            /* errata TX-1: ensure TX done is issued */
            dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
            status = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
            ver = GLOSSY_STANDARD_VERSION;
        }

        if (status != DWT_SUCCESS) {
            LOG_ERROR("Glossy start: Failed to TX\n");
            glossy_stop();
        }
        else {
            if (ver == GLOSSY_TX_ONLY_VERSION)
                STATETIME_MONITOR(dw1000_statetime_schedule_tx(ts_tx_4ns));
            else
                STATETIME_MONITOR(dw1000_statetime_schedule_txrx(ts_tx_4ns, rx_delay_uus));
        }

    } else {
        // the node is not initiator.

        // go in rx state and wait for packet reception
        if (GLOSSY_RX_OPT && start_at_dtu_time) {
            dwt_setdelayedtrxtime(start_time_dtu);                   // delay transmission at given **ts**
            g_context.ts_start = start_time_dtu;
            dwt_setrxtimeout(0); // no rx-timeout set
            status = dwt_rxenable(DWT_START_RX_DELAYED | DWT_IDLE_ON_DLY_ERR);
        }
        else {
            dwt_setrxtimeout(0); // no rx-timeout set
            g_context.ts_start = dwt_readsystimestamphi32();        // TODO: take into account the turnaround time
            status = dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }

        if (status != DWT_SUCCESS) {
            LOG_ERROR("Glossy start: Failed to RX\n");
            glossy_stop();
        }
        else {
            STATETIME_MONITOR(dw1000_statetime_schedule_rx(g_context.ts_start));
        }
    }
    g_context.state = GLOSSY_STATE_ACTIVE;
    return GLOSSY_STATUS_SUCCESS;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_stop(void)
{
    // if already stopped, avoid doing the computation again
    if (g_context.state == GLOSSY_STATE_OFF) {
        return g_context.n_rx;
    }
    // stop any radio activity
    dwt_forcetrxoff();

    // memorise the stop time
    g_context.ts_stop = dwt_readsystimestamphi32();
    g_context.state   = GLOSSY_STATE_OFF;
    STATETIME_MONITOR(dw1000_statetime_abort(g_context.ts_stop); dw1000_statetime_stop());

    if (!is_glossy_initiator()) {
        if (GLOSSY_GET_SYNC(g_context.pkt_header.config) == GLOSSY_WITH_SYNC) {
            // compute initiator's first TX time
            if (g_context.tref_updated) {
                if (g_context.n_slots > 0 && GLOSSY_DYNAMIC_SLOT_ESTIMATE) {
                    g_context.tref = g_context.tref -
                        g_context.tref_relay_cnt * g_context.slot_sum / g_context.n_slots;
                } else {
                    // it wasn't possible to estimate any slot or
                    // the dynamic slot estimation is not set
                    g_context.tref = g_context.tref -
                        g_context.tref_relay_cnt * g_context.slot_duration;
                }
            }
        }
        if (g_context.n_rx > 0 && g_context.pkt_payload != NULL) {
            // copy the received payload
            memcpy(g_context.pkt_payload,
                   clean_buffer + GLOSSY_PAYLOAD_OFFSET,
                   GLOSSY_PAYLOAD_LEN(g_context.psdu_len));
        }
        if (g_context.pkt_payload == NULL) {
            LOG_DEBUG("App didn't provide place to store the payload.\n");
        }
    }

    #if GLOSSY_STATS
    g_context.stats.n_rx += g_context.n_rx;
    g_context.stats.n_tx += g_context.n_tx;
    #endif /* GLOSSY_STATS */

    g_context.pkt_payload = NULL;
    // TODO: resume interrupted tasks
    return g_context.n_rx;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_is_active(void)
{
    //
    return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_get_n_rx(void)
{
    return g_context.n_rx;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_get_n_tx(void)
{
    return g_context.n_tx;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_get_payload_len(void)
{
  if (g_context.psdu_len == GLOSSY_PSDU_NONE)
      return GLOSSY_UNKNOWN_PAYLOAD_LEN;
  else
      return GLOSSY_PAYLOAD_LEN(g_context.psdu_len);
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_is_t_ref_updated(void)
{
    return g_context.tref_updated;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t glossy_get_t_ref(void)
{
    return radio_to_rtimer_ts(g_context.tref);
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_t_ref_dtu(void)
{
    return g_context.tref;
}
/*---------------------------------------------------------------------------*/
uint16_t glossy_get_initiator_id(void)
{
    return g_context.pkt_header.initiator_id;
}
/*---------------------------------------------------------------------------*/
glossy_sync_t
glossy_get_sync_opt(void)
{
    //
    return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_get_max_payload_len()
{
    return GLOSSY_MAX_PAYLOAD_LEN;
}
/*---------------------------------------------------------------------------*/
#if GLOSSY_STATS
void glossy_get_stats(glossy_stats_t* stats)
{
    memcpy(stats, &g_context.stats, sizeof(glossy_stats_t));
}
/*---------------------------------------------------------------------------*/
void glossy_stats_print()
{
    LOG("GLOSSY_STATS_1",
            "n_rx %"PRIu16", n_tx %"PRIu16", "
            "relay_cnt_first_rx %"PRIu8"\n",
            g_context.stats.n_rx, g_context.stats.n_tx,
            g_context.stats.relay_cnt_first_rx);
    LOG("GLOSSY_STATS_2",
            "n_bad_length %"PRIu16", n_bad_header %"   PRIu16", n_payload_mismatch %"       PRIu16", n_length_mismatch %"PRIu16"\n",
            g_context.stats.n_bad_length, g_context.stats.n_bad_header, g_context.stats.n_payload_mismatch, g_context.stats.n_length_mismatch);
    LOG("GLOSSY_STATS_3",
            "rx_to %"       PRIu16", rx_preamble_to %"  PRIu16", rx_frame_to %"         PRIu16"\n",
            g_context.stats.rx_timeouts, g_context.stats.n_preamble_to, g_context.stats.n_rfw_to);
    LOG("GLOSSY_STATS_4",
            "n_rx_err %"    PRIu16", rx_phr_err %"      PRIu16", rx_sfd_err %"          PRIu16"\n",
            g_context.stats.n_rx_err, g_context.stats.n_phr_err, g_context.stats.n_sfd_to);
    LOG("GLOSSY_STATS_5",
            "rx_rs_err %"   PRIu16", rx_fcs_err %"      PRIu16", rx_frames_rejected %"  PRIu16"\n",
            g_context.stats.n_rs_err, g_context.stats.n_fcs_err, g_context.stats.ff_rejects);
}
/*---------------------------------------------------------------------------*/
uint8_t glossy_get_relay_cnt_first_rx(void)
{
    return g_context.tref_relay_cnt;
}
#endif /* GLOSSY_STATS */
/*---------------------------------------------------------------------------*/
void glossy_debug_print(void)
{
    if(g_context.tref_updated) {
        LOG("GLOSSY_FLOOD_DEBUG",
                "n_T_slots %"PRIu8
                ", relay_cnt_t_ref %"PRIu8 ", relay_cnt_last_rx %"PRIu8
                ", T_slot %"PRIu32", tref %"PRIu32
                ", slot_duration %"PRIu32", T_active %"PRIu32"\n",
                g_context.n_slots,
                g_context.tref_relay_cnt, g_context.relay_cnt_last_rx,
                (g_context.n_slots > 0 && GLOSSY_DYNAMIC_SLOT_ESTIMATE) ?
                    (g_context.slot_sum / g_context.n_slots) :
                    g_context.slot_duration,
                g_context.tref,
                g_context.slot_duration,
                g_context.state == GLOSSY_STATE_ACTIVE ?
                    0 :
                    g_context.ts_stop - g_context.ts_start);
    }
}
/*---------------------------------------------------------------------------*/
void glossy_version_print(void)
{
    LOG("GLOSSY_VERSION", "%x , with dynamic slot est. %d, "
        "SmartTx %d\n",
            GLOSSY_VERSION, GLOSSY_DYNAMIC_SLOT_ESTIMATE,
            DW1000_SMART_TX_POWER_6M8);
}
/*---------------------------------------------------------------------------*/
/*                           UTILITY FUNCTIONS IMPLEMENTATION                */
/*---------------------------------------------------------------------------*/
/* Gives an RTIMER timestamp from a radio timestamp; radio_ts must be acquired
  * BEFORE calling radio_to_rtimer_ts()
  *
  * Source: uwb_contiki/examples/tdoa_mh_new/tdoa.c
  */
static rtimer_clock_t
radio_to_rtimer_ts(uint32_t radio_ts)
{
    uint32_t current_radio_ts = dwt_readsystimestamphi32();
    rtimer_clock_t current_rtimer_ts = RTIMER_NOW();
    double elapsed_s;

    elapsed_s = (double)(current_radio_ts - radio_ts) / 1000000000 * 4.0064102564;
    return current_rtimer_ts - (elapsed_s * RTIMER_SECOND);
}
/*---------------------------------------------------------------------------*/
static inline bool is_glossy_initiator()
{
    return g_context.pkt_header.initiator_id == node_id;
}
/*---------------------------------------------------------------------------*/
static inline int
glossy_resume_flood()
{
    // This function is only used by the initiator with the standard Glossy
    uint32_t ts_tx_4ns;
    int status;

    // use previous tx SFD as reference and schedule TX two slots after
    ts_tx_4ns = g_context.ts_last_tx +
        (g_context.slot_duration * 2) - tx_antenna_delay_4ns;

    g_context.pkt_header.relay_cnt += 2;
    glossy_update_hdr(&g_context.pkt_header, clean_buffer);
    dwt_writetxdata(g_context.psdu_len, clean_buffer, 0);
    dwt_writetxfctrl(g_context.psdu_len, 0, 0);

    if (GLOSSY_GET_VERSION(g_context.pkt_header.config) ==
        GLOSSY_STANDARD_VERSION) {

        // if RX optimisation is enabled, set the RX after TX delay to a non-zero value
        uint32_t rx_delay_uus = glossy_get_rx_delay_uus(g_context.psdu_len);
        dwt_setrxaftertxdelay(rx_delay_uus);

        // set RX timeout so that the initiator can resume the flood
        // even if it misses the RX
        uint16_t rx_timeout_uus = glossy_get_rx_timeout_uus(g_context.psdu_len);
        dwt_setrxtimeout(rx_timeout_uus);

        // start delayed TX
        dwt_setdelayedtrxtime(ts_tx_4ns);
        /* errata TX-1: ensure TX done is issued */
        dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, PMSC_CTRL0_TXCLKS_125M);
        status = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
        if (status == DWT_SUCCESS) {
            STATETIME_MONITOR(dw1000_statetime_schedule_txrx(ts_tx_4ns, rx_delay_uus));
        }

    } else {

        // This branch should never be entered. Return error.
        status = DWT_ERROR;

    }
    return status;
}
/*---------------------------------------------------------------------------*/
static void
glossy_context_init()
{

    g_context.pkt_header.initiator_id = GLOSSY_UNKNOWN_INITIATOR;
    g_context.pkt_header.relay_cnt    = 0;
    g_context.pkt_header.max_n_tx     = 0;
    g_context.pkt_header.config       = 0x0;
    // set specific values on config
    GLOSSY_SET_SYNC(g_context.pkt_header.config, GLOSSY_UNKNOWN_SYNC);
    GLOSSY_SET_VERSION(g_context.pkt_header.config, GLOSSY_UNKNOWN_VERSION);
    /*-----------------------------------------------------------------------*/
    g_context.psdu_len = GLOSSY_PSDU_NONE; // no packet received or sent
    g_context.pkt_payload = NULL;
    /*-----------------------------------------------------------------------*/
    g_context.tref = 0;
    g_context.tref_relay_cnt = 0;
    g_context.tref_updated = false;
    /*-----------------------------------------------------------------------*/
    g_context.ts_last_rx = 0;
    g_context.ts_last_tx = 0;
    g_context.relay_cnt_last_rx = 0;
    g_context.relay_cnt_last_tx = 0;
    /*-----------------------------------------------------------------------*/
    g_context.n_tx = 0;
    g_context.n_rx = 0;
    /*-----------------------------------------------------------------------*/
    g_context.slot_sum = 0;
    g_context.n_slots  = 0;
    g_context.slot_duration = 0;
    /*-----------------------------------------------------------------------*/
    g_context.state = GLOSSY_STATE_OFF;
    /*-----------------------------------------------------------------------*/
    g_context.ts_stop  = 0;
    /*-----------------------------------------------------------------------*/
    g_context.status_reg = 0;
    g_context.ppm_offset = 0;
    // do NOT overwrite this:
    // g_context.ts_start = dw1000 timestamp when glossy started
}
/*---------------------------------------------------------------------------*/
#if GLOSSY_STATS
static void
glossy_stats_init()
{
    // Glossy dw1000 specific
    g_context.stats.relay_cnt_first_rx = 0;
    /*-----------------------------------------------------------------------*/
    g_context.stats.n_rx_err  = 0;
    g_context.stats.n_phr_err = 0;
    g_context.stats.n_sfd_to  = 0;
    g_context.stats.n_rs_err  = 0;
    g_context.stats.n_fcs_err = 0;
    g_context.stats.ff_rejects  = 0;
    /*-----------------------------------------------------------------------*/
    g_context.stats.n_rfw_to      = 0;
    g_context.stats.n_preamble_to = 0;
    /*-----------------------------------------------------------------------*/
    // legacy
    g_context.stats.rx_timeouts   = 0;
    g_context.stats.n_bad_length  = 0;
    g_context.stats.n_bad_header  = 0;
    g_context.stats.n_length_mismatch  = 0;
    g_context.stats.n_payload_mismatch = 0;
    g_context.stats.n_rx  = 0;
    g_context.stats.n_tx  = 0;
}
#endif /* GLOSSY_STATS */
/*---------------------------------------------------------------------------*/
static inline void
tref_update(const uint32_t tref, const uint8_t tref_relay_cnt)
{
    g_context.tref = tref;
    g_context.tref_relay_cnt = tref_relay_cnt;
    g_context.tref_updated = true;
}
/*---------------------------------------------------------------------------*/
#if GLOSSY_DYNAMIC_SLOT_ESTIMATE
static inline void
add_slot(const uint32_t slot_duration)
{
    g_context.slot_sum += slot_duration;
    g_context.n_slots += 1;
}
#endif /* GLOSSY_DYNAMIC_SLOT_ESTIMATE */
/*---------------------------------------------------------------------------*/

/* Determines the slot duration based on the packet length and the current configuration.
 * Returns value with 4ns time unit.
 */
static uint32_t
calc_slot_duration(uint16_t psdu_len)
{
    // Slot duration is computed as:
    //   - frame on air duration
    //   - time to download and upload the packet over SPI
    //   - software delay that depends on the amount of logging
    return (
        dw1000_estimate_tx_time(dw1000_get_current_cfg(), psdu_len, false) +
           2400*psdu_len + // measured SPI upload+download speed
           ((GLOSSY_LOG_LEVEL<=GLOSSY_LOG_ERROR_LEVEL) ? 270000 : 500000)
           //((GLOSSY_LOG_LEVEL<=GLOSSY_LOG_ERROR_LEVEL) ? 500000 : 1500000)    // use larger values when testing with the replier
           ) / 4; // we use ns instead of uwb ns here for speed, the actual slot duration will be slightly different
}
/*---------------------------------------------------------------------------*/
static inline
uint32_t glossy_get_rx_delay_uus(uint16_t psdu_len)
{
#if GLOSSY_RX_OPT
    // TODO: maybe it's better to check that the resulting value is not greater than the
    // slot (underflow). In that case I think it's better not to have an inline function

    // Here we compute the delay before turning on RX after the previous TX.

    // We should start listening in one slot duration minus the packet on-air
    // duration and minus a guard time that accommodates various HW and SW delays
    // and clock inaccuracies.

    return (GLOSSY_DTU_4NS_TO_UUS(g_context.slot_duration) -
              (dw1000_estimate_tx_time(dw1000_get_current_cfg(), psdu_len, false) / 1024   // ns to uus, approx.
              ) - GLOSSY_RX_OPT_GUARD_UUS);
#else
    return 0; // no rx after tx delay when no optimisation is enabled: turn on immediately
#endif
}
/*---------------------------------------------------------------------------*/
static inline
uint16_t glossy_get_rx_timeout_uus(uint16_t psdu_len)
{
#if GLOSSY_RX_OPT

    // In this case, the timer is started after the rx_delay followed by the
    // previous TX.
    //
    // From that moment, we need to wait for the entire packet on-air duration
    // plus the guard time we used to turn the radio on beforehand, plus
    // a small guard time at the end to compensate for the propagation time and
    // clock inaccuracies

    return dw1000_estimate_tx_time(dw1000_get_current_cfg(), psdu_len, false) / 1024   // ns to uus, approx.
             + GLOSSY_RX_OPT_GUARD_UUS + GLOSSY_RX_TIMEOUT_GUARD_UUS;
#else
    // In this case, the timer is started right after the previous TX has finished.
    //
    // From that moment, we need wait for the entire slot duration plus a small
    // guard time at the end to compensate for the propagation time and clock
    // inaccuracies
    return GLOSSY_DTU_4NS_TO_UUS(g_context.slot_duration) + GLOSSY_RX_TIMEOUT_GUARD_UUS;
#endif
}
/*---------------------------------------------------------------------------*/
static inline glossy_status_t
glossy_validate_header(const glossy_header_t* rcvd_header)
{
    // General rule:
    // received packet has to provide a new value for the field in case
    // it is the first packet received, or it must have its field values
    // equal to the packet stored within the context from a previous reception

    // check on VER field
    // Received packet has to state the version option
    if (GLOSSY_GET_VERSION(g_context.pkt_header.config) == GLOSSY_UNKNOWN_VERSION &&
        GLOSSY_GET_VERSION(rcvd_header->config) == GLOSSY_UNKNOWN_VERSION) {
        return GLOSSY_STATUS_FAIL;
    }
    if (GLOSSY_GET_VERSION(g_context.pkt_header.config) != GLOSSY_UNKNOWN_VERSION &&
        GLOSSY_GET_VERSION(rcvd_header->config) !=
        GLOSSY_GET_VERSION(g_context.pkt_header.config)) {
        return GLOSSY_STATUS_FAIL;
    }
    // check on SYNC field
    // Received packet has to state the sync option
    if (GLOSSY_GET_SYNC(g_context.pkt_header.config) == GLOSSY_UNKNOWN_SYNC &&
        GLOSSY_GET_SYNC(rcvd_header->config) == GLOSSY_UNKNOWN_SYNC) {
        return GLOSSY_STATUS_FAIL;
    }
    if (GLOSSY_GET_SYNC(g_context.pkt_header.config) != GLOSSY_UNKNOWN_SYNC &&
        GLOSSY_GET_SYNC(rcvd_header->config) !=
        GLOSSY_GET_SYNC(g_context.pkt_header.config)) {
        return GLOSSY_STATUS_FAIL;
    }
    // check on max_n_tx
    if (g_context.pkt_header.max_n_tx == GLOSSY_UNKNOWN_N_TX_MAX &&
        rcvd_header->max_n_tx == GLOSSY_UNKNOWN_N_TX_MAX) {
        return GLOSSY_STATUS_FAIL;
    }
    if (g_context.pkt_header.max_n_tx != GLOSSY_UNKNOWN_N_TX_MAX &&
        rcvd_header->max_n_tx != g_context.pkt_header.max_n_tx) {
        return GLOSSY_STATUS_FAIL;
    }
    // check on initiator id
    if (g_context.pkt_header.initiator_id == GLOSSY_UNKNOWN_INITIATOR &&
        rcvd_header->initiator_id == GLOSSY_UNKNOWN_INITIATOR) {
        return GLOSSY_STATUS_FAIL;
    }
    if (g_context.pkt_header.initiator_id != GLOSSY_UNKNOWN_INITIATOR &&
        rcvd_header->initiator_id != g_context.pkt_header.initiator_id) {
        return GLOSSY_STATUS_FAIL;
    }

    return GLOSSY_STATUS_SUCCESS;
}
/*---------------------------------------------------------------------------*/
float
glossy_get_ppm_offset()
{
    return g_context.ppm_offset;
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_status_reg(void)
{
    return g_context.status_reg;
}

uint32_t
glossy_get_slot_duration(uint8_t payload_len) {

    uint8_t psdu_len = sizeof(glossy_header_t) + payload_len + DW1000_CRC_LEN;
    return calc_slot_duration(psdu_len);
}

uint32_t
glossy_get_round_duration() {
    return g_context.state == GLOSSY_STATE_ACTIVE ?
        0 :
        g_context.ts_stop - g_context.ts_start;
}
