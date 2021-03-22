#include "usbd_cdc_core.h"
#include "usbd_conf.h"

uint16_t DW_VCP_Init(void);
uint16_t DW_VCP_DeInit(void);
uint16_t DW_VCP_Ctrl(uint32_t Cmd, uint8_t *Buf, uint32_t Len);
uint16_t DW_VCP_DataTx(uint8_t *Buf, uint32_t Len);
uint16_t DW_VCP_DataRx(uint8_t *Buf, uint32_t Len);
int usb_init(void);
