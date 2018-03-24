#include "usbd_cdc_core.h"
#include "usbd_conf.h"

/* These are external variables imported from CDC core to be used for IN
   transfer management. */
extern uint8_t APP_Rx_Buffer[];   /* Write CDC received data in this buffer.
                                     These data will be sent over USB IN endpoint
                                     in the CDC core functions. */
extern uint32_t APP_Rx_ptr_in;    /* Increment this pointer or roll it back to
                                     start address when writing received data
                                     in the buffer APP_Rx_Buffer. */
extern uint32_t APP_Rx_length;

uint16_t DW_VCP_Init(void);
uint16_t DW_VCP_DeInit(void);
uint16_t DW_VCP_Ctrl(uint32_t Cmd, uint8_t *Buf, uint32_t Len);
uint16_t DW_VCP_DataTx(uint8_t *Buf, uint32_t Len);
uint16_t DW_VCP_DataRx(uint8_t *Buf, uint32_t Len);
int usb_init(void);
