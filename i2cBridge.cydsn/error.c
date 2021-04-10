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

/// General structure for I2C errors. These are all defined as uint8_t or
/// uint8_t arrays to ensure a packed structure.
typedef struct I2cError
{
    /// Error type.
    uint8_t type;
    
    /// I2C status; refer to I2cGen2Status enum.
    uint8_t status;
    
    /// The last low level I2C driver status mask.
    uint8_t driverStatus[4];
    
    /// The unique callsite ID that describes the function that triggered the
    /// error.
    uint8_t callsite[2];
    
} I2cError;


// === CONSTANTS ===============================================================

/// Message format for the CLI I2C error.
char const* CliI2cErrorFormat = "[Err|I2C]: %02x:$08x @ %04x\r\n";


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


int error_makeSystemError(uint8_t buffer[], uint16_t size, uint8_t systemStatus, uint16_t callsite)
{
    int status = -1;
    if (g_mode == ErrorMode_Global)
    {
    }
    else if (g_mode == ErrorMode_Cli)
    {
    }
    return status;
}


int error_makeUpdaterError(uint8_t buffer[], uint16_t size, uint8_t updaterStatus, uint16_t callsite)
{
    int status = -1;
    if (g_mode == ErrorMode_Global)
    {
    }
    else if (g_mode == ErrorMode_Cli)
    {
    }
    return status;
}


int error_makeI2cError(uint8_t buffer[], uint16_t size, I2cGen2Status i2cStatus, uint16_t callsite)
{
    int dataSize = -1;
    uint32_t driverStatus = i2cGen2_getLastDriverStatus();
    if (g_mode == ErrorMode_Global)
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
    else if (g_mode == ErrorMode_Cli)
    {
    }
    return dataSize;
}


int error_makeUartError(uint8_t buffer[], uint16_t size, uint8_t uartStatus, uint16_t callsite)
{
    int status = -1;
    if (g_mode == ErrorMode_Global)
    {
    }
    else if (g_mode == ErrorMode_Cli)
    {
    }
    return status;
}


/* [] END OF FILE */
