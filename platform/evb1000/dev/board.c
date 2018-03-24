/*
 * \file
 *		EVB1000 Board Source File
 *
 */

#include "board.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
/*---------------------------------------------------------------------------*/
void
rtc_init(void)
{
  /* Enable PWR and BKP clocks */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

  /* Allow access to BKP Domain */
  PWR_BackupAccessCmd(ENABLE);

  /* Reset Backup Domain */
  BKP_DeInit();

  /* Enable LSE */
  RCC_LSEConfig(RCC_LSE_ON);
  /* Wait till LSE is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) {}

  /* Select LSE as RTC Clock Source */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

  /* Enable RTC Clock */
  RCC_RTCCLKCmd(ENABLE);

  /* Wait for RTC registers synchronization */
  RTC_WaitForSynchro();

  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();

  /* Enable the RTC Alarm Interrupt */
  RTC_ITConfig(RTC_IT_ALR, ENABLE);

  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();

  /* Set RTC prescaler: set RTC period to 1sec */
  /* RTC period = RTCCLK / RTC_PR = (32.768 KHz) / (32767 + 1) */
  /* RTC_SetPrescaler(32767); */

  /* Set RTC prescaler: set RTC period to 1 / 32.768 Hz */
  /* RTC period = RTCCLK / RTC_PR = (32.768 KHz) / (0 + 1) */
  RTC_SetPrescaler(0);

  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();
}
/*---------------------------------------------------------------------------*/
void
rcc_init(void)
{
  
  ErrorStatus HSEStartUpStatus;
  RCC_ClocksTypeDef RCC_ClockFreq;

  /* RCC system reset(for debug purpose) */
  RCC_DeInit();

  /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  HSEStartUpStatus = RCC_WaitForHSEStartUp();

  if(HSEStartUpStatus != ERROR)
  {
    /* Enable Prefetch Buffer */
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

    /****************************************************************/
    /* HSE= up to 25MHz (on EVB1000 is 12MHz)
     * HCLK=72MHz, PCLK2=72MHz, PCLK1=36MHz */
    /****************************************************************/
    /* Flash 2 wait state */
    FLASH_SetLatency(FLASH_Latency_2);
    /* HCLK = SYSCLK */
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    /* PCLK2 = HCLK */
    RCC_PCLK2Config(RCC_HCLK_Div1);
    /* PCLK1 = HCLK/2 */
    RCC_PCLK1Config(RCC_HCLK_Div2);
    /*  ADCCLK = PCLK2/4 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* Configure PLLs */
    /* PLL2 configuration: PLL2CLK = (HSE / 4) * 8 = 24 MHz */
    RCC_PREDIV2Config(RCC_PREDIV2_Div4);
    RCC_PLL2Config(RCC_PLL2Mul_8);

    /* Enable PLL2 */
    RCC_PLL2Cmd(ENABLE);

    /* Wait till PLL2 is ready */
    while (RCC_GetFlagStatus(RCC_FLAG_PLL2RDY) == RESET){}

    /* PLL1 configuration: PLLCLK = (PLL2 / 3) * 9 = 72 MHz */
    RCC_PREDIV1Config(RCC_PREDIV1_Source_PLL2, RCC_PREDIV1_Div3);

    RCC_PLLConfig(RCC_PLLSource_PREDIV1, RCC_PLLMul_9);

    /* Enable PLL */
    RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET){}

    /* Select PLL as system clock source */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while (RCC_GetSYSCLKSource() != 0x08){}
  }

  RCC_GetClocksFreq(&RCC_ClockFreq);
}
/*---------------------------------------------------------------------------*/
void
gpio_init(void)
{
  GPIO_InitTypeDef gpio_conf;

  /* Enable GPIOs clocks */
  RCC_APB2PeriphClockCmd(
    RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | 
    RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);

  /* Configure all unused GPIO port pins in Analog Input Mode (floating input
   * trigger OFF) to reduce the power consumption and increase the device 
   * immunity against EMI/EMC */
  gpio_conf.GPIO_Pin = GPIO_Pin_All;
  gpio_conf.GPIO_Mode = GPIO_Mode_AIN;
  GPIO_Init(GPIOA, &gpio_conf);
  GPIO_Init(GPIOB, &gpio_conf);
  GPIO_Init(GPIOC, &gpio_conf);
  GPIO_Init(GPIOD, &gpio_conf);
  GPIO_Init(GPIOE, &gpio_conf);

  /* The GPIO Port Pins that are used should be properly initialized after
   * a call to this function */
}
/*---------------------------------------------------------------------------*/
void
nvic_init(void)
{
  /* NVIC Selected Configuration:
   *  - 4 Bits for Priority with Preemption
   *  - No sub-priority
   */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
}
