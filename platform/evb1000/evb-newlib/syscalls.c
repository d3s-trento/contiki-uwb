#include <sys/stat.h>
#include "stm32f10x.h"
#include "deca_usb.h"

caddr_t
_sbrk(int incr)
{
  extern char _ebss; /* Defined by the linker. */
  static char *heap_end;
  char *prev_heap_end;
  char *stack;

  if(heap_end == 0) {
    heap_end = &_ebss;
  }

  prev_heap_end = heap_end;

  stack = (char *)__get_MSP();

  if(heap_end + incr > stack) {
    return (caddr_t)-1;
  }

  heap_end += incr;

  return (caddr_t)prev_heap_end;
}

int
_write(int file, char *ptr, int len)
{
  if(DW_VCP_DataTx((uint8_t *)ptr, len) == USBD_OK)
    return len;

  return -1;
}

int
_close(int file)
{
  return 0;
}

int
_lseek(__attribute__((unused)) int file, __attribute__((unused)) int ptr, __attribute__((unused)) int dir)
{
  return 0;
}

int
_read(int file, char *ptr, int len)
{
  // scanf calls this function, but we don't support it
  return 0;
}

int
_fstat(__attribute__((unused)) int file, __attribute__((unused)) struct stat *st)
{
  return 0;
}

int
_isatty(__attribute__((unused)) int file)
{
  return 1;
}
