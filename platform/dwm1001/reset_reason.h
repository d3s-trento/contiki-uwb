
#ifndef __RESET_REASON_H__
#define __RESET_REASON_H__

typedef struct reset_reason_t {
  uint8_t reset_pin       :1;
  uint8_t watchdog        :1;
  uint8_t soft_reset      :1;
  uint8_t lockup          :1;

  uint8_t system_off      :1;
  uint8_t lp_comp         :1;
  uint8_t debug_interface :1;
  uint8_t nfc             :1;
    
} reset_reason_t;

extern reset_reason_t last_reset_reason;

#endif /* __RESET_REASON_H__ */

