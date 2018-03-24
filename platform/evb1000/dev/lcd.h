/**
 * \file
 *         EVB1000 LCD Header File
 */

#ifndef LCD_H_
#define LCD_H_
/*---------------------------------------------------------------------------*/
#include "board.h"
/*---------------------------------------------------------------------------*/
#define LCD_CMD_CLR_DISPLAY 	0x1
#define LCD_CMD_RETURN_HOME 	0x2
/*---------------------------------------------------------------------------*/
void lcd_init(void);
void lcd_write(uint32_t len, uint8_t rs_enable, const uint8_t *buf);
void lcd_display_str(const char *str);
/*---------------------------------------------------------------------------*/
#endif /* LCD_H_ */
