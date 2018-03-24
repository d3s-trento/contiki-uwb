/*
 * \file
 *		EVB1000 Board Header File
 *
 */

#ifndef BOARD_H_
#define BOARD_H_
/*---------------------------------------------------------------------------*/
#include "stm32f10x.h"
/*---------------------------------------------------------------------------*/
/* LED GPIO Port and Pins */
#define EVB1000_LEDS_PORT GPIOC
#define EVB1000_LED_5     GPIO_Pin_6 /* LED PC6 (Yellow) */
#define EVB1000_LED_6     GPIO_Pin_7 /* LED PC7 (Red) */
#define EVB1000_LED_7     GPIO_Pin_8 /* LED PC8 (Yellow) */
#define EVB1000_LED_8     GPIO_Pin_9 /* LED PC9 (Red) */
#define EVB1000_LED_ALL   (EVB1000_LED_5 | EVB1000_LED_6 | EVB1000_LED_7 | EVB1000_LED_8)
/*---------------------------------------------------------------------------*/
/* Some files include leds.h before us, so we need to get rid of defaults in
 * leds.h before we provide correct definitions */
#undef LEDS_GREEN
#undef LEDS_YELLOW
#undef LEDS_RED
#undef LEDS_CONF_ALL

#define LEDS_YELLOW     1 /* LED PC6 (Yellow) */
#define LEDS_RED        2 /* LED PC7 (Red) */
#define LEDS_GREEN      4 /* LED PC8 (Yellow) */
#define LEDS_ORANGE     8 /* LED PC9 (Red) */
#define LEDS_CONF_ALL   15

/* Notify various examples that we have LEDs */
#define PLATFORM_HAS_LEDS        1
/*---------------------------------------------------------------------------*/
/* Platform dependent DW1000 Ports and Pins */
/* DW1000 SPI 1 */
#define DW1000_SPI 				SPI1
#define DW1000_SPI_PRESCALER    SPI_BaudRatePrescaler_8
#define DW1000_SPI_PORT			GPIOA
#define DW1000_SPI_SCK			GPIO_Pin_5
#define DW1000_SPI_MISO			GPIO_Pin_6
#define DW1000_SPI_MOSI 		GPIO_Pin_7
/* DW1000 SPI Chip Select */
#define DW1000_CS_PORT			GPIOA
#define DW1000_CS_PIN			GPIO_Pin_4
/* DW1000 IRQ Pin */
#define DW1000_IRQ_PORT 		GPIOB
#define DW1000_IRQ_PIN 			GPIO_Pin_5
#define DW1000_IRQ_EXTI 		EXTI_Line5
#define DW1000_IRQ_EXTI_PORT 	GPIO_PortSourceGPIOB
#define DW1000_IRQ_EXTI_PIN 	GPIO_PinSource5
#define DW1000_IRQ_EXTI_IRQN	EXTI9_5_IRQn
/* DW1000 RST Pin */
#define DW1000_RST_PORT			GPIOA
#define DW1000_RST_PIN			GPIO_Pin_0
#define DW1000_RST_EXTI			EXTI_Line0
#define DW1000_RST_EXTI_PORT  	GPIO_PortSourceGPIOA
#define DW1000_RST_EXTI_PIN   	GPIO_PinSource0
#define DW1000_RST_EXTI_IRQN  	EXTI0_IRQn
/*---------------------------------------------------------------------------*/
#define LCD_SPI					SPI2
#define LCD_SPI_PRESCALER 		SPI_BaudRatePrescaler_128
#define LCD_SPI_PORT			GPIOB
#define LCD_SPI_SCK 			GPIO_Pin_13
#define LCD_SPI_MISO			GPIO_Pin_14	/* Not required */
#define LCD_SPI_MOSI			GPIO_Pin_15	
#define LCD_CS_PORT 			GPIOB
#define LCD_CS_PIN 				GPIO_Pin_12
#define LCD_RW_PORT				GPIOB
#define LCD_RW_PIN 				GPIO_Pin_10
#define LCD_RS_PORT				GPIOB
#define LCD_RS_PIN 				GPIO_Pin_11
/*---------------------------------------------------------------------------*/
#define USB_SUPPORT 1
/*---------------------------------------------------------------------------*/
void rcc_init(void);
void gpio_init(void);
void nvic_init(void);
void rtc_init(void);
/*---------------------------------------------------------------------------*/
#endif /* BOARD_H_ */
