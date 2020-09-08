#ifndef ADDR_BT2UWB
#define ADDR_BT2UWB

#include <stdint.h>

/* Convert 6-byte BT address to 8-byte UWB address */
void dwm1001_bt2uwb_addr(const uint8_t* bt_addr, uint8_t* uwb_addr);

#endif
