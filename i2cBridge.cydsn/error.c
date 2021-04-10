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

/// Calculate the number of ASCII hex characters needed to display numbers that
/// are represented by x bytes.
#define NUM_HEX_CHAR(x)                 (x * 2u)


// === TYPE DEFINES ============================================================

/// General structure for I2C errors.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
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


/// General structure for the error system status.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct Status
{
    /// Error type.
    uint8_t type;
    
    /// Current error mode.
    uint8_t mode;
    
    uint8_t systemCount[2];
    
    uint8_t updaterCount[2];
    
    uint8_t uartCount[2];
    
    uint8_t i2cCount[2];
    
} Status;


/// CLI meta data: used to assist in generating CLI error messages.
typedef struct MetaData
{
    /// ID/name of the error type.
    char* id;
    
    /// Message format.
    char* format;
    
    /// Number of ASCII hex characters needed to display the numeric fields.
    uint8_t hexCharCount;
    
} MetaData;


// === CONSTANTS ===============================================================

/// CLI error header.
char const* CliErrorHeader = "Err";

/// CLI meta data for the different error types.
MetaData const CliMetaData[] =
{
    // ErrorType_I2c.
    {
        "I2C",
        "[%s|%s] %02x.$08x @%04x\r\n",
        NUM_HEX_CHAR(sizeof(I2cError)),
    },
    
    // ErrorType_Status.
    {
        "Stat",
        "[%s|%s] %02x\r\n",
        NUM_HEX_CHAR(sizeof(I2cError)),
    },
};


// === GLOBALS =================================================================

/// Indicates the current error mode. Default: legacy.
static ErrorMode g_mode = ErrorMode_Legacy;


/// Counter that tracks the number of times each ErrorType occurred (with the
/// exception of ErrorType_Status because this is a general status).
static uint16_t g_errorCount[] =
{
    0,
    0,
    0,
    0,
};


// === PUBLIC FUNCTIONS ========================================================

ErrorMode error_getMode(void)
{
    return g_mode;
}


void error_setMode(ErrorMode mode)
{
    g_mode = mode;
}


void error_tally(ErrorType type)
{
    if (type != ErrorType_Status)
        g_errorCount[type]++;
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
