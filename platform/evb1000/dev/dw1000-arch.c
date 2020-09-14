/**
 * \file
 *      Platform Dependent DW1000 Driver Source File
 *
 */

#include "contiki.h"
#include "sys/clock.h"
#include "board.h"
#include "leds.h"
#include "dw1000-arch.h"
#include <stdio.h>
#include "dw1000.h"
/*---------------------------------------------------------------------------*/
#include "spix.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "lcd.h"
/*---------------------------------------------------------------------------*/
/* Declaration of static functions */
static void dw1000_select(void);
static void dw1000_deselect(void);
/*---------------------------------------------------------------------------*/
/* Set the DW1000 ISR to NULL by default */
static dw1000_isr_t dw1000_isr = NULL;
/*---------------------------------------------------------------------------*/
/* DW1000 Interrupt pin handler */
void
EXTI9_5_IRQHandler(void)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  do {
    dw1000_isr();
  } while(GPIO_ReadInputDataBit(DW1000_IRQ_PORT, DW1000_IRQ_PIN) == 1);

  /* Clear DW1000 EXTI Line 5 Pending Bit */
  EXTI_ClearITPendingBit(DW1000_IRQ_EXTI);

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
static inline int8_t
dw1000_get_exti_int_status(uint32_t exti_line)
{
  /* Check the parameters */
  //assert_param(IS_GET_EXTI_LINE(exti_line));

  return (int8_t)(EXTI->IMR & exti_line);
}
/*---------------------------------------------------------------------------*/
int8_t
dw1000_disable_interrupt(void)
{

  int8_t irqn_status;

  irqn_status = dw1000_get_exti_int_status(DW1000_IRQ_EXTI);
  
  if(irqn_status != 0) {
    NVIC_DisableIRQ(DW1000_IRQ_EXTI_IRQN);
  }
  return irqn_status;
}
/*---------------------------------------------------------------------------*/
void
dw1000_enable_interrupt(int8_t irqn_status)
{
  if(irqn_status != 0) {
    NVIC_EnableIRQ(DW1000_IRQ_EXTI_IRQN);
  }
}
/*---------------------------------------------------------------------------*/
static void
dw1000_select(void)
{
  GPIO_ResetBits(DW1000_CS_PORT, DW1000_CS_PIN);
}
/*---------------------------------------------------------------------------*/
static void
dw1000_deselect(void)
{
  GPIO_SetBits(DW1000_CS_PORT, DW1000_CS_PIN);
}
/*---------------------------------------------------------------------------*/
void
dw1000_set_isr(dw1000_isr_t new_dw1000_isr)
{
  int8_t irqn_status;

  irqn_status = dw1000_get_exti_int_status(DW1000_IRQ_EXTI);

  /* Disable DW1000 EXT Interrupt */
  if(irqn_status) {
    dw1000_disable_interrupt();
  }
  /* Set ISR Handler */
  dw1000_isr = new_dw1000_isr;

  /* Re-enable DW1000 EXT Interrupt state */
  if(irqn_status) {
    dw1000_enable_interrupt(irqn_status);
  }
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_open(void)
{

}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_close(void)
{

}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_read(uint16_t hdrlen, const uint8_t *hdrbuf, uint32_t len, uint8_t *buf)
{
  uint32_t i;
  int8_t irqn_status;

  /* Disable DW1000 EXT Interrupt */
  irqn_status = dw1000_disable_interrupt();

  /* Clear SPI1 Chip Select */
  dw1000_select();

  /* Write Header */
  for(i = 0; i < hdrlen; i++) {
    /* Send data over SPI */
    SPI_I2S_SendData(SPI1, hdrbuf[i]);
    /* Wait for the RX Buffer to be filled */
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    /* Clear Flags */
    SPI_I2S_ReceiveData(SPI1);
  }

  /* Read body */
  for(i = 0; i < len; i++) {
    /* Send dummy data over SPI */
    SPI_I2S_SendData(SPI1, 0xFF);
    /* Wait for the RX Buffer to be filled */
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    /* Receive data */
    buf[i] = SPI_I2S_ReceiveData(SPI1);
  }

  /* Set SPI1 Chip Select */
  dw1000_deselect();

  /* Re-enable DW1000 EXT Interrupt state */
  dw1000_enable_interrupt(irqn_status);
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_write(uint16_t hdrlen, const uint8_t *hdrbuf, uint32_t len, const uint8_t *buf)
{
  uint32_t i;
  int8_t irqn_status;

  /* Disable DW1000 EXT Interrupt */
  irqn_status = dw1000_disable_interrupt();

  /* Clear SPI1 Chip Select */
  dw1000_select();

  /* Write Header */
  for(i = 0; i < hdrlen; i++) {
    /* Send data over SPI */
    SPI_I2S_SendData(SPI1, hdrbuf[i]);

    /* Wait for the RX Buffer to be filled */
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

    /* Clear Flags */
    SPI_I2S_ReceiveData(SPI1);
  }

  /* Write body */
  for(i = 0; i < len; i++) {
    /* Send data over SPI */
    SPI_I2S_SendData(SPI1, buf[i]);

    /* Wait for the RX Buffer to be filled */
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

    /* Clear Flags */
    SPI_I2S_ReceiveData(SPI1);
  }

  /* Set SPI1 Chip Select */
  dw1000_deselect();

  /* Re-enable DW1000 EXT Interrupt state */
  dw1000_enable_interrupt(irqn_status);
}
/*---------------------------------------------------------------------------*/
void
dw1000_set_spi_bit_rate(uint16_t brate)
{
  spix_change_speed(SPI1, brate);
}
/*---------------------------------------------------------------------------*/
void
dw1000_arch_init()
{
  SPI_InitTypeDef spi1_conf;
	GPIO_InitTypeDef gpio_conf;
  EXTI_InitTypeDef exti_conf;
  NVIC_InitTypeDef nvic_conf;

  /* SPI1 Configuration */
  spi1_conf.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  spi1_conf.SPI_Mode = SPI_Mode_Master;
  spi1_conf.SPI_DataSize = SPI_DataSize_8b;
  spi1_conf.SPI_CPOL = SPI_CPOL_Low;
  spi1_conf.SPI_CPHA = SPI_CPHA_1Edge;
  spi1_conf.SPI_NSS = SPI_NSS_Soft;
  spi1_conf.SPI_BaudRatePrescaler = DW1000_SPI_SLOW;
  spi1_conf.SPI_FirstBit = SPI_FirstBit_MSB;
  spi1_conf.SPI_CRCPolynomial = 7;

  /* Init SPI1 */
  spix_init(SPI1, &spi1_conf);

  /* Disable SPI1 SS Output -- is this required? */
  SPI_SSOutputCmd(SPI1, DISABLE);

  /* DW1000 CS Pin Configuration */
  gpio_conf.GPIO_Pin = DW1000_CS_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(DW1000_CS_PORT, &gpio_conf);
  GPIO_SetBits(DW1000_CS_PORT, DW1000_CS_PIN);

  /* DW1000 RST Pin Configuration */
  gpio_conf.GPIO_Pin = DW1000_RST_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_AIN;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(DW1000_RST_PORT, &gpio_conf);


  /* For initialisation, DW1000 clocks must be temporarily set to crystal speed.
   * After initialisation SPI rate can be increased for optimum performance.
   */

  dw1000_arch_reset(); /* Target specific drive of RSTn line into DW1000 low for a period.*/

  if (dwt_readdevid() != DWT_DEVICE_ID) {
    printf("Radio sleeping?\n");
    dw1000_arch_wakeup();
    dw1000_arch_reset();
  }

  if(dwt_initialise(DWT_LOADUCODE | DWT_READ_OTP_PID | DWT_READ_OTP_LID |
                    DWT_READ_OTP_BAT | DWT_READ_OTP_TMP) 
          == DWT_ERROR) {
    lcd_display_str("INIT FAILED");
    while(1) {
      /* If the init function fails, we stop here */
      // XXX handle this in a better way!
    }
  }
  spix_change_speed(DW1000_SPI, DW1000_SPI_FAST);

  /* DW1000 IRQ Pin Configuration */
  /* 
   * Enable DW1000 IRQ Pin for external interrupt.
   * NOTE: The DW1000 IRQ Pin should be Pull Down to
   * prevent unnecessary EXT Interrupt while DW1000
   * goes to sleep mode.
   */
  gpio_conf.GPIO_Pin = DW1000_IRQ_PIN;
  gpio_conf.GPIO_Mode =  GPIO_Mode_IPD;
  GPIO_Init(DW1000_IRQ_PORT, &gpio_conf);

  /* Connect EXTI Line to DW1000 IRQ Pin */
  GPIO_EXTILineConfig(DW1000_IRQ_EXTI_PORT, DW1000_IRQ_EXTI_PIN);

  /* Configure DW1000 IRQ EXTI line */
  exti_conf.EXTI_Line = DW1000_IRQ_EXTI;
  exti_conf.EXTI_Mode = EXTI_Mode_Interrupt;
  exti_conf.EXTI_Trigger = EXTI_Trigger_Rising;
  exti_conf.EXTI_LineCmd = ENABLE;
  EXTI_Init(&exti_conf);

  /* Set DW1000 IRQ EXT Interrupt to the lowest priority */
  nvic_conf.NVIC_IRQChannel = DW1000_IRQ_EXTI_IRQN;
  nvic_conf.NVIC_IRQChannelPreemptionPriority = 15;
  nvic_conf.NVIC_IRQChannelSubPriority = 0;
  nvic_conf.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&nvic_conf);

}
/*---------------------------------------------------------------------------*/
void
dw1000_arch_reset()
{
  GPIO_InitTypeDef gpio_conf;

  /* Set RST Pin as an output */
  gpio_conf.GPIO_Pin = DW1000_RST_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_AIN;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(DW1000_RST_PORT, &gpio_conf);

  /* Clear the RST pin to reset the DW1000 */
  GPIO_ResetBits(DW1000_RST_PORT, DW1000_RST_PIN);

  clock_delay_usec(1);

  /* Set the RST pin back as an input */
  gpio_conf.GPIO_Pin = DW1000_RST_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_AIN;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(DW1000_RST_PORT, &gpio_conf);

  /* Sleep 2 ms to get the DW1000 restarted */
  clock_wait(2);
}
/*---------------------------------------------------------------------------*/
void dw1000_arch_wakeup() {
    /* To wake up the DW1000 we keep the SPI CS line low for (at least) 500us.
     * This can be achieved with a long read SPI transaction.*/
  uint8_t wakeup_buffer[600];
  dwt_readfromdevice(0x0, 0x0, 600, wakeup_buffer);
  /* Need 5ms for XTAL to start and stabilise
   * (could wait for PLL lock IRQ status bit !!!)
   * NOTE: Polling of the STATUS register is not possible
   * unless frequency is < 3MHz */
  clock_wait(5);
}
