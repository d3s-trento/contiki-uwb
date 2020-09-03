#ifndef _DECA_DRIVER_STATE_H_
#define _DECA_DRIVER_STATE_H_

#include "deca_device_api.h"

// Structure to hold device data
typedef struct
{
    uint32      partID ;            // IC Part ID - read during initialisation
    uint32      lotID ;             // IC Lot ID - read during initialisation
    uint8       vBatP ;             // IC V bat read during production and stored in OTP (Vmeas @ 3V3)
    uint8       tempP ;             // IC V temp read during production and stored in OTP (Tmeas @ 23C)
    uint8       longFrames ;        // Flag in non-standard long frame mode
    uint8       otprev ;            // OTP revision number (read during initialisation)
    uint32      txFCTRL ;           // Keep TX_FCTRL register config
    uint32      sysCFGreg ;         // Local copy of system config register
    uint8       dblbuffon;          // Double RX buffer mode flag
    uint8       wait4resp ;         // wait4response was set with last TX start command
    uint16      sleep_mode;         // Used for automatic reloading of LDO tune and microcode at wake-up
    uint16      otp_mask ;          // Local copy of the OTP mask used in dwt_initialise call
    dwt_cb_data_t cbData;           // Callback data structure
    dwt_cb_t    cbTxDone;           // Callback for TX confirmation event
    dwt_cb_t    cbRxOk;             // Callback for RX good frame event
    dwt_cb_t    cbRxTo;             // Callback for RX timeout events
    dwt_cb_t    cbRxErr;            // Callback for RX error events
} dwt_local_data_t ;

extern dwt_local_data_t *pdw1000local;

#endif  // _DECA_DRIVER_STATE_H_
