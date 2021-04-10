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

#include <string.h>

#include "utility.h"


// === DEFINES =================================================================


// === TYPE DEFINES ============================================================

typedef struct I2cError
{
    uint8_t type;
    uint8_t status;
    uint8_t driverStatus[4];
    uint8_t callsite[2];
} I2cError;


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


int error_makeI2cError(uint8_t buffer[], uint16_t size, I2cGen2Status i2cStatus, uint16_t callsite, bool cli)
{
    int dataSize = -1;
    uint32_t driverStatus = i2cGen2_getLastDriverStatus();
    if (cli)
    {
    }
    else
    {
        if (size >= sizeof(I2cError))
        {
            I2cError error =
            {
                ErrorType_I2c,
                i2cStatus.errorOccurred,
                {
                    BYTE_3_32_BIT(driverStatus),
                    BYTE_2_32_BIT(driverStatus),
                    BYTE_1_32_BIT(driverStatus),
                    BYTE_0_32_BIT(driverStatus),
                },
                {
                    HI_BYTE_16_BIT(callsite),
                    LO_BYTE_16_BIT(callsite),
                },
            };
            memcpy(buffer, &error, sizeof(error));
            dataSize = sizeof(error);
        }
    }
    return dataSize;
}


int error_makeUartError(uint8_t buffer[], uint16_t size, uint8_t uartStatus, uint16_t callsite, bool cli)
{
    int status = -1;
    return status;
}


/* [] END OF FILE */
