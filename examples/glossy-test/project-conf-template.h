#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
#include "radio-conf.h"
/*
 * Define Glossy parameters in this file, then
 * rename it to project-conf.h to use it.
 * The define the macro with the desired value.
 *
 * If using the simgen, a project-conf.h will be automatically
 * generated for each simulation. In this case, make sure
 * no previous project-conf.h file is present in the application
 * directory
 *
 */


/*---------------------------------------------------------------------------*/
/*                          RADIO CONFIGURABLE PARAMETERS                    */
/*---------------------------------------------------------------------------*/
/* parameters for smart tx and tx power
 * DW1000_CONF_SMART_TX_POWER_6M8:     0 (disabled) |
 *                                     1 (enabled)  (DEFAULT)
 */
#define DW1000_CONF_SMART_TX_POWER_6M8
/*---------------------------------------------------------------------------*/
/*
 * DW1000_TX_POWER: <power value>
 *
 * Define a quartet of bytes representing transmission powers
 * under the scenarios defined in the DW1000 User Manual
 *
 * If not defined, the power configuration based on
 * the smart tx power feature suggested in the User Manual is
 * set.
 *
 */
#define DW1000_CONF_TX_POWER
/*---------------------------------------------------------------------------*/
/*                          GLOSSY_TEST CONFIGURABLE PARAMETERS              */
/*---------------------------------------------------------------------------*/
/*
 *  INITIATOR_ID    the initiator's node id.
 *
 */
#define INITIATOR_ID
/* GLOSSY_TEST_CONF_PAYLOAD_DATA_LEN: <byte>    // define payload dimension
 *                                              // between 0 and 109 (DEFAULT)
 *
 */
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_N_TX      the maximum number of transmissions performed
 *                  by each node within a flood.
 *
 */
#define GLOSSY_N_TX
/*---------------------------------------------------------------------------*/
#define GLOSSY_TEST_CONF_PAYLOAD_DATA_LEN
/*---------------------------------------------------------------------------*/
/*                          GLOSSY CONFIGURATION                             */
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_VERSION_EMPLOYED_CONF:   GLOSSY_TX_ONLY_VERSION |
 *                                 GLOSSY_STANDARD_VERSION
 * DEFAULT: GLOSSY_TX_ONLY_VERSION
 */
#define GLOSSY_VERSION_EMPLOYED_CONF
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF:    0 (DEFAULT) |
 *                                       1
 *
 * If set to 1 the slot is dynamically estimated based on Rx-Tx and Tx-Rx
 * pairs.
 *
 * If set to 0, the slot is set based on the estimated transmission time
 * of the first frame received.
 */
#define GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF
/*---------------------------------------------------------------------------*/
/*
 * GLOSSY_LOG_LEVEL_CONF:   GLOSSY_LOG_NONE_LEVEL  |
 *                          GLOSSY_LOG_ALL_LEVELS  |
 *                          GLOSSY_LOG_INFO_LEVEL  |
 *                          GLOSSY_LOG_DEBUG_LEVEL |
 *                          GLOSSY_LOG_ERROR_LEVEL
 *
 * DEFAULT: GLOSSY_LOG_NONE_LEVEL
 */
#define GLOSSY_LOG_LEVEL_CONF
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
