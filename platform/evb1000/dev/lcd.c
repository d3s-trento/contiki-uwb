/**
 * \file
 *         EVB1000 LCD Source File
 */

#include "contiki.h"
#include "sys/clock.h"
#include "board.h"
#include "lcd.h"
#include "spix.h"
/*---------------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
/*---------------------------------------------------------------------------*/
#include <string.h>
/*---------------------------------------------------------------------------*/
static void
lcd_select(void)
{
  GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN);
}
/*---------------------------------------------------------------------------*/
static void
lcd_deselect(void)
{
  GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN);
}
/*---------------------------------------------------------------------------*/
static void
lcd_clear(void)
{
  uint8_t cmd;

  /* Return cursor home and clear screen. */
  cmd = LCD_CMD_RETURN_HOME;
  lcd_write(1, 0, &cmd);

  cmd = LCD_CMD_CLR_DISPLAY;
  lcd_write(1, 0, &cmd);
}
/*---------------------------------------------------------------------------*/
void
lcd_write(uint32_t len, uint8_t rs_enable, const uint8_t *buf)
{
  uint32_t i;

  /* Set/Clear RS Pin */
  GPIO_WriteBit(LCD_RS_PORT, LCD_RS_PIN, (rs_enable) ? Bit_SET : Bit_RESET);

  /* Clear SPI LCD Chip Select */
  lcd_select();

  for(i = 0; i < len; i++) {
    /* Send data over SPI */
    SPI_I2S_SendData(SPI2, buf[i]);

    /* Wait for the RX Buffer to be filled */
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);

    /* Do we need this? */
    SPI_I2S_ReceiveData(SPI2);
  }

  /* Clear RS */
  GPIO_ResetBits(LCD_RS_PORT, LCD_RS_PIN);

  /* Set SPI LCD Chip Select */
  lcd_deselect();

  /* Wait 2 ms 
   * Note: The commands to clear the display or return the cursor home
   * take 1.08 ms. Therefore, we wait 2 ms if required
   */
  if(len == 1 && buf[0] & 0x3) {
    clock_wait(2);
  }
}
/*---------------------------------------------------------------------------*/
void
lcd_display_str(const char *str)
{
  /* Clear display */
  lcd_clear();

  /* Write the string to display. */
  lcd_write(strlen(str), 1, (const uint8_t *)str);
}
/*---------------------------------------------------------------------------*/
void
lcd_init(void)
{
  SPI_InitTypeDef spi2_conf;
  GPIO_InitTypeDef gpio_conf;
  unsigned char initseq[9] = {0x39, 0x14, 0x55, 0x6D, 0x78, 0x38, 0x0C, 0x01, 0x06};

  /* SPI2 Configuration */
  SPI_StructInit(&spi2_conf);
  spi2_conf.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  spi2_conf.SPI_Mode = SPI_Mode_Master;
  spi2_conf.SPI_DataSize = SPI_DataSize_8b;
  spi2_conf.SPI_CPOL = SPI_CPOL_High;
  spi2_conf.SPI_CPHA = SPI_CPHA_2Edge;
  spi2_conf.SPI_NSS = SPI_NSS_Soft;
  spi2_conf.SPI_BaudRatePrescaler = LCD_SPI_PRESCALER;
  spi2_conf.SPI_FirstBit = SPI_FirstBit_MSB;
  spi2_conf.SPI_CRCPolynomial = 7;

  /* Init SPI2 */
  spix_init(SPI2, &spi2_conf);

  /* Disable SPI2 SS Output  -- is this required? */
  SPI_SSOutputCmd(SPI2, DISABLE);

  /* LCD CS Pin Configuration */
  gpio_conf.GPIO_Pin = LCD_CS_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(LCD_CS_PORT, &gpio_conf);
  GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN);

  /* LCD RW and RS Pins Configuration */
  gpio_conf.GPIO_Pin = LCD_RW_PIN | LCD_RS_PIN;
  gpio_conf.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(LCD_RW_PORT, &gpio_conf);
  GPIO_ResetBits(LCD_RW_PORT, LCD_RW_PIN | LCD_RS_PIN);

  /* Wait for LCD to power on */
  clock_wait(10);

  /* Write initialisation sequence */
  lcd_write(9, 0, (const uint8_t *) &initseq);
  clock_wait(10);

  /* Clear display */
  lcd_clear();
}
/*---------------------------------------------------------------------------*/
