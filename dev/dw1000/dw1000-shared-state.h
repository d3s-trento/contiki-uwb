/* This header file is supposed to be used by the DW1000 driver internally
 */


#ifndef DW1000_SHARED_STATE_H
#define DW1000_SHARED_STATE_H

#include "dw1000-config-struct.h"

extern bool dw1000_is_sleeping; /* true when the radio is in DEEP SLEEP mode */
extern struct dw1000_all_config dw1000_cached_config; /* current cached radio configuration */

#endif
