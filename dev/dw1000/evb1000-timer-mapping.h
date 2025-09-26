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
 *      Enrico Soprana <enrico.soprana@unitn.it>
 */
#ifndef EVB1000_TIMER_MAPPING_H
#define EVB1000_TIMER_MAPPING_H

#include <stdint.h>

/**
 * @brief Initialize the timestamp mapping (after this you should also set tm_set_actual_epoch_start_mcu and tm_set_actual_epoch_start_dw1000)
 */
void tm_init();

/**
 * @brief Get the current timestamp of the mcu
 *
 * @return the timestamp of the mcu
 */
uint16_t tm_get_now();

/**
 * @brief Get the last mcu timestamp saved 
 *
 * @return the last timestamp saved
 */
uint16_t tm_get_timestamp();

/**
 * @brief Set the specified time as the start of the epoch, note that this should be the time at which you set the dw1000 timer
 *
 * @param actual_epoch_start_4ns The timer (in 4 uwb ns) at which the timer is set
 */
void tm_set_actual_epoch_start_dw1000(uint32_t actual_epoch_start_4ns);
/**
 * @brief Set the current mcu time as the corresponding time for actual_epoch_start_4ns
 */
void tm_set_actual_epoch_start_mcu();

/**
 * @brief Save the current time
 */
void tm_set_timestamp_mcu();

/**
 * @brief Get the time elapsed between the specified radio_start_4ns and the mcu timestamp specified
 *
 * @param radio_start_4ns The time (in uwb 4ns time) at which the action started
 * @param timestamp_mcu The mcu timestamp taken as soon as the action ended
 * @return the time, in (real) ns, elapsed between the two provided timestamps
 */
uint32_t tm_get_elapsed_time_ts_ns(uint32_t radio_start_4ns, uint16_t timestamp_mcu);
/**
 * @brief Get the time elapsed between the specified radio_start_4ns and the current mcu timestamp
 *
 * @param radio_start_4ns The time (in uwb 4ns time) at which the action started
 * @return the time, in (real) ns, elapsed between the two provided timestamps
 */
uint32_t tm_get_elapsed_time_ns(uint32_t radio_start_4ns);
/**
 * @brief Get the time elapsed between the specified radio_start_4ns and the current mcu timestamp
 *
 * @param radio_start_4ns The time (in uwb 4ns time) at which the action started
 * @return the time, in (uwb) 4ns, elapsed between the two provided timestamps
 */
uint32_t tm_get_elapsed_time_4ns(uint32_t radio_start_4ns);

/**
 * @brief Update the timer estimations assuming known, obtained through a rfto interrupt, corresponding timestamps for both the dw1000 radio and the mcu
 *
 * @param end_timestamp_dw1000 the radio timestamp
 * @param end_timestamp_mcu the mcu timestamp
 */
void tm_update_rxrfto(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu);

/**
 * @brief Update the timer estimation assuming, obtained through a pto interrupt, known corresponding timestamp for both the dw1000 radio and the mcu
 *
 * @param end_timestamp_dw1000 the radio timestamp
 * @param end_timestamp_mcu the mcu timestamp
 */
void tm_update_rxpto(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu);

/**
 * @brief Update the timer estimation assuming, obtained through a tx done interrupt, known corresponding timestamp for both the dw1000 radio and the mcu
 *
 * @param end_timestamp_dw1000 the radio timestamp
 * @param end_timestamp_mcu the mcu timestamp
 * @param len the length of the packet to consider
 */
void tm_update_tx(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu, uint16_t len);

/**
 * @brief Returns the current estimation of the mcu's clock period
 *
 * @return the mcu clock period
 */
float tm_get_mcu_period_ns();
/**
 * @brief Returns the last mcu clock period seen
 *
 * @return the last mcu clock period seen
 */
float tm_get_last_mcu_val();

/**
 * @brief Partially resets the timers. Must be used between two epochs
 */
void tm_reset();

#endif
