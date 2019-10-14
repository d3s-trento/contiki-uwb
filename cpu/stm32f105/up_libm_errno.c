/* Replace undefined __errno ptr to enable the math.h library in the STM32 */
#include <errno.h>
int math_errno;
int *__errno()
{
return &math_errno;
}
