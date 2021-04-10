/* ========================================
 *
 * UICO, 2021
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

// === DEPENDENCIES ============================================================

#include "error.h"


// === DEFINES =================================================================


// === GLOBALS =================================================================

/// Indicates the current error mode. Default: legacy.
static ErrorMode g_mode = ErrorMode_Legacy;


// === PUBLIC FUNCTIONS ========================================================

ErrorMode error_getMode(void)
{
    return g_mode;
}


void error_setMode(ErrorMode mode)
{
    g_mode = mode;
}


int error_makeSystemError(uint8_t buffer[], uint16_t size, uint8_t systemStatus, uint16_t callsite, bool cli)
{
    int status = -1;
    return status;
}


int error_makeUpdaterError(uint8_t buffer[], uint16_t size, uint8_t updaterStatus, uint16_t callsite, bool cli)
{
    int status = -1;
    return status;
}


int error_makeI2cError(uint8_t buffer[], uint16_t size, I2cGen2Status i2cStatus, uint32_t driverStatus, uint16_t callsite, bool cli)
{
    int status = -1;
    return status;
}


int error_makeUartError(uint8_t buffer[], uint16_t size, uint8_t uartStatus, uint16_t callsite, bool cli)
{
    int status = -1;
    return status;
}


/* [] END OF FILE */
