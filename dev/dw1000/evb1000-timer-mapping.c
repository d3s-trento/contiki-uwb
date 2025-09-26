#include "evb1000-timer-mapping.h"

#include "../../cpu/stm32f105/lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x/stm32f10x.h"
#include "../../cpu/stm32f105/lib/STM32F10x_StdPeriph_Driver/inc/stm32f10x_tim.h"
#include "../../cpu/stm32f105/lib/STM32F10x_StdPeriph_Driver/inc/stm32f10x_rcc.h"

#include "dw1000-conv.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MEDIAN_WINDOW 9

struct interrupt_timer_t {
  bool initialized;
  uint32_t last_dw1000;
  uint16_t last_mcu;
};

static struct {
  struct interrupt_timer_t rxrfto;
  struct interrupt_timer_t rxpto;
  struct interrupt_timer_t tx_done;

  uint16_t timestamp_mcu;

  float mcu_clock_period_win[MEDIAN_WINDOW];
  uint8_t mcu_clock_period_win_counter[MEDIAN_WINDOW];

  uint8_t median_counter;

  uint16_t last_tx_len;

  // Debugging only
  float last_mcu_val;
} context;

#define CURRENT_MCU_CLOCK (context.mcu_clock_period_win[MEDIAN_WINDOW/2])

/**
 * @brief Initializes the basic structure for an interrupt mapping
 *
 * @param it The structure to initialize
 */
static void it_init(struct interrupt_timer_t * it) {
  it->initialized = false;
  it->last_mcu = 0;
  it->last_dw1000 = 0;
}

void tm_init() {
  TIM_TimeBaseInitTypeDef baseInitStruct = {
    .TIM_Prescaler = 3,
    .TIM_CounterMode = TIM_CounterMode_Up,
    .TIM_Period = ~((uint16_t)0),
    .TIM_ClockDivision = TIM_CKD_DIV1,
    .TIM_RepetitionCounter = 0,
  };

  TIM_DeInit(TIM4);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

  TIM_TimeBaseInit(TIM4, &baseInitStruct);
  TIM_InternalClockConfig(TIM4);
  TIM_SetCounter(TIM4, 0);
  TIM_Cmd(TIM4, ENABLE);
  TIM_ARRPreloadConfig(TIM4, ENABLE);

  context.last_tx_len = 0;

  // Initialize the window used for the median to what we expect the ratio to be
  uint8_t i;
  for (i=0; i<MEDIAN_WINDOW; ++i) {
    context.mcu_clock_period_win[i] = 1000./18.; // 72MHz/(.TIM_Prescaler+1)
    context.mcu_clock_period_win_counter[i] = i;
  }

  context.median_counter = 0;

  // Initialize all the interrupt mappings
  it_init(&context.rxrfto);
  it_init(&context.rxpto);
  it_init(&context.tx_done);
}

static void tm_insert_mcu_est(float val, uint8_t idx) {

  // Find the index at which we want to insert the new value
  uint8_t i;
  for (i=0; context.mcu_clock_period_win_counter[i] != idx; ++i);

  // Move all the values at the left of the index to 1 spot to the right
  for(;i>0; --i) {
    context.mcu_clock_period_win[i] = context.mcu_clock_period_win[i-1];
    context.mcu_clock_period_win_counter[i] = context.mcu_clock_period_win_counter[i-1];
  }

  // Set the value as the first element of the window with the specified index
  context.mcu_clock_period_win[0] = val;
  context.mcu_clock_period_win_counter[0] = idx;

  // Reorder the window based on the value (as to have the median at the center of the array
  for(i=1; (i<MEDIAN_WINDOW) && (context.mcu_clock_period_win[i] > context.mcu_clock_period_win[i-1]); ++i) {
    uint8_t ti = context.mcu_clock_period_win_counter[i-1];
    float tv = context.mcu_clock_period_win[i-1];

    context.mcu_clock_period_win[i-1] = context.mcu_clock_period_win[i];
    context.mcu_clock_period_win_counter[i-1] = context.mcu_clock_period_win_counter[i];

    context.mcu_clock_period_win[i] = tv;
    context.mcu_clock_period_win_counter[i] = ti;
  }
}

uint16_t tm_get_now() {
  return TIM_GetCounter(TIM4);
}

uint16_t tm_get_timestamp() {
  return context.timestamp_mcu;
}

void tm_set_actual_epoch_start_dw1000(uint32_t actual_epoch_start_4ns) {
  context.rxrfto.initialized = false;
  context.rxrfto.last_dw1000 = actual_epoch_start_4ns;
  context.rxrfto.last_mcu = 0;
}

void tm_set_actual_epoch_start_mcu() {
  context.rxrfto.initialized = true;
  context.rxrfto.last_mcu = tm_get_now();
}

void tm_set_timestamp_mcu() {
  context.timestamp_mcu = tm_get_now();
}

float tm_get_mcu_period_ns() {
  return CURRENT_MCU_CLOCK;
}

float tm_get_last_mcu_val() {
  return context.last_mcu_val;
}

static uint32_t it_estimate_mcu(const struct interrupt_timer_t * const it, uint32_t timestamp_dw1000) {
    uint32_t radio_rx_time_4ns = timestamp_dw1000 - it->last_dw1000;
    uint16_t time_mcu = it->last_mcu;
    time_mcu += radio_rx_time_4ns * DWT_TICK_TO_NS_32 / CURRENT_MCU_CLOCK;

    return time_mcu;
}

static uint32_t it_elapsed_ns(const struct interrupt_timer_t * const it, uint32_t base_dw1000, uint16_t timestamp_mcu) {
    uint16_t time_mcu = it_estimate_mcu(it, base_dw1000);
    time_mcu = timestamp_mcu - time_mcu;

    return time_mcu * CURRENT_MCU_CLOCK;
}

static void it_update(struct interrupt_timer_t * it, uint32_t timestamp_dw1000, uint16_t timestamp_mcu) {
  // If the interrupt timer was not yet initialized, initialize it and return (without estimating the mcu_clock_period)
  if (!it->initialized) {
    it->initialized = true;
    it->last_dw1000 = timestamp_dw1000;
    it->last_mcu = timestamp_mcu;
    return;
  }

  uint16_t mcu_diff = timestamp_mcu;
  mcu_diff -= it->last_mcu;
  uint32_t radio_diff = timestamp_dw1000;
  radio_diff -= it->last_dw1000;

  // Check if the distance betwene the parts is reasonable enough that we can be sure that we the mcu timer did not overflow
  if (CURRENT_MCU_CLOCK * (50000 / DWT_TICK_TO_NS_32) > radio_diff) {
    float new_val = radio_diff*DWT_TICK_TO_NS_32/mcu_diff;

    tm_insert_mcu_est(new_val, context.median_counter);
    context.median_counter = (context.median_counter + 1) % MEDIAN_WINDOW;

    context.last_mcu_val = new_val;
  }

  it->last_dw1000 = timestamp_dw1000;
  it->last_mcu = timestamp_mcu;
}

uint32_t tm_get_elapsed_time_ts_ns(uint32_t radio_start_4ns, uint16_t timestamp_mcu) {
  return it_elapsed_ns(&context.rxrfto, radio_start_4ns, timestamp_mcu);
}

uint32_t tm_get_elapsed_time_ns(uint32_t radio_start_4ns) {
    return tm_get_elapsed_time_ts_ns(radio_start_4ns, context.timestamp_mcu);
}

uint32_t tm_get_elapsed_time_4ns(uint32_t radio_start_4ns) {
  return tm_get_elapsed_time_ns(radio_start_4ns)/DWT_TICK_TO_NS_32;
}

void tm_update_rxrfto(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu) {
  it_update(&context.rxrfto, end_timestamp_dw1000, end_timestamp_mcu);

  context.rxrfto.initialized = true;
  context.rxrfto.last_dw1000 = end_timestamp_dw1000;
  context.rxrfto.last_mcu = end_timestamp_mcu;
}

void tm_update_rxpto(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu) {
  it_update(&context.rxpto, end_timestamp_dw1000, end_timestamp_mcu);
}

void tm_update_tx(uint32_t end_timestamp_dw1000, uint16_t end_timestamp_mcu, uint16_t len){
  if (context.last_tx_len == len) {
    it_update(&context.tx_done, end_timestamp_dw1000, end_timestamp_mcu);
  } else {
    context.tx_done.initialized = true;
    context.tx_done.last_dw1000 = end_timestamp_dw1000;
    context.tx_done.last_mcu = end_timestamp_mcu;
  }

  context.last_tx_len = len;
}

void tm_reset() {
  it_init(&context.rxpto);
  it_init(&context.rxrfto);
  it_init(&context.tx_done);
}

