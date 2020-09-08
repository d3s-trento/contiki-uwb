#include <stddef.h>
#include <string.h>
#include "bt2uwb-addr.h"

/* Convert 6-byte BT address to 8-byte UWB address */
void dwm1001_bt2uwb_addr(const uint8_t* bt_addr, uint8_t* uwb_addr) {
  if (uwb_addr == NULL)
    return;

  uwb_addr[0] = 0xde;
  uwb_addr[1] = 0xca;
  memcpy(&uwb_addr[2], bt_addr, 6);
}
