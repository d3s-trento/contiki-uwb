
#ifndef __BATTERY_READ_H__
#define __BATTERY_READ_H__

typedef void (*battery_read_callback_t) (const uint16_t millivolts);

uint8_t battery_read_init(battery_read_callback_t cb);

uint8_t battery_read_start();

#endif /* __BATTERY_READ_H__ */
