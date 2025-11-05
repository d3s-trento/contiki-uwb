#ifndef SAMU_RADIO_DEFAULTS_H
#define SAMU_RADIO_DEFAULTS_H

#include "dw1000-conv.h"

#ifdef SAMU_CONF_DEFAULT_RXGUARD
#define SAMU_DEFAULT_RXGUARD SAMU_CONF_DEFAULT_RXGUARD
#else
#define SAMU_DEFAULT_RXGUARD  (10*UUS_TO_DWT_TIME_32) // receivers guard time
#endif


#endif
