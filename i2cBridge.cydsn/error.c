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

/// Counter type; used to tally error counts.
typedef uint16_t count_t;


/// General structure for System errors.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct SystemError
{
    /// Error type.
    uint8_t type;
    
    /// Status mask.
    uint8_t status;
    
    /// The unique callsite ID that describes the function that triggered the
    /// error.
    uint8_t callsite[2];
    
} SystemError;


/// General structure for Updater errors.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct UpdaterError
{
    /// Error type.
    uint8_t type;
    
    /// Status mask.
    uint8_t status;
    
    /// The unique callsite ID that describes the function that triggered the
    /// error.
    uint8_t callsite[2];
    
} UpdaterError;


/// General structure for UART errors.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct UartError
{
    /// Error type.
    uint8_t type;
    
    /// Status mask.
    uint8_t status;
    
    /// The unique callsite ID that describes the function that triggered the
    /// error.
    uint8_t callsite[2];
    
} UartError;


/// General structure for I2C errors.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct I2cError
{
    /// Error type.
    uint8_t type;
    
    /// I2C status; refer to I2cTouchStatus enum.
    uint8_t status;
    
    /// The unique callsite ID that describes the function that triggered the
    /// error.
    uint8_t callsite[2];
    
    /// The last low-level I2C driver status mask.
    uint8_t driverStatus[2];
    
    /// The last return value from the low-level driver function.
    uint8_t driverReturnValue[2];
    
} I2cError;


/// General structure for the error system's current mode.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct Mode
{
    /// Error type.
    uint8_t type;
    
    /// Current error mode.
    uint8_t mode;
} Mode;


/// General structure for the error system statistics.
/// Note: these are all defined as uint8_t or uint8_t arrays to ensure a packed
/// structure.
typedef struct Stats
{
    /// Error type.
    uint8_t type;
    
    /// System error count.
    uint8_t systemCount[sizeof(count_t)];
    
    /// Updater error count.
    uint8_t updaterCount[sizeof(count_t)];
    
    /// UART protocol error count.
    uint8_t uartCount[sizeof(count_t)];
    
    /// I2C protocol error count.
    uint8_t i2cCount[sizeof(count_t)];
    
} Stats;


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
char const* CliErrorHeader = "ERR";

/// CLI meta data for the different error types.
MetaData const CliMetaData[] =
{
    // ErrorType_System.
    {
        "Sys",
        "[%s|%s] %02x @%04x\r\n",
        NUM_HEX_CHAR(sizeof(SystemError)),
    },
    
    // ErrorType_Updater.
    {
        "Up",
        "[%s|%s] %02x @%04x\r\n",
        NUM_HEX_CHAR(sizeof(UpdaterError)),
    },
    
    // ErrorType_System.
    {
        "UART",
        "[%s|%s] %02x @%04x\r\n",
        NUM_HEX_CHAR(sizeof(UartError)),
    },
    
    // ErrorType_I2c.
    {
        "I2C",
        "[%s|%s] %02x.%04x.%04x @%04x\r\n",
        NUM_HEX_CHAR(sizeof(I2cError)),
    },
    
    // ErrorType_Mode.
    {
        "Mode",
        "[%s|%s] %02x\r\n",
        NUM_HEX_CHAR(sizeof(Mode)),
    },
    
    // ErrorType_Stats.
    {
        "Stat",
        "[%s|%s] %04x.%04x.%04x.%04x\r\n",
        NUM_HEX_CHAR(sizeof(Stats)),
    },
};


// === GLOBALS =================================================================

/// Indicates the current error mode. Default: legacy.
/// @TODO: revert to ErrorMode_Legacy.
static ErrorMode g_mode = ErrorMode_Global;


/// Counter that tracks the number of times each ErrorType occurred (with the
/// exception of ErrorType_Status because this is a general status).
static count_t g_errorCount[] =
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
    if (type < ErrorType_Mode)
        g_errorCount[type]++;
}


int error_makeSystemErrorMessage(uint8_t buffer[], uint16_t size, SystemStatus systemStatus, uint16_t callsite)
{
    int dataSize = -1;
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(SystemError))
        {
            SystemError error =
            {
                ErrorType_System,
                systemStatus.value,
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
        // @TODO: implement.
    }
    return dataSize;
}


int error_makeUpdaterErrorMessage(uint8_t buffer[], uint16_t size, uint8_t updaterStatus, uint16_t callsite)
{
    int dataSize = -1;
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(UpdaterError))
        {
            UpdaterError error =
            {
                ErrorType_Updater,
                updaterStatus,
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
        // @TODO: implement.
    }
    return dataSize;
}


int error_makeI2cErrorMessage(uint8_t buffer[], uint16_t size, I2cTouchStatus i2cStatus, uint16_t callsite)
{
    int dataSize = -1;
    
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(I2cError))
        {
            uint16_t driverStatus = i2cTouch_getLastDriverStatusMask();
            uint16_t driverReturnValue = i2cTouch_getLastDriverReturnValue();
            I2cError error =
            {
                ErrorType_I2c,
                i2cStatus.value,
                {
                    HI_BYTE_16_BIT(callsite),
                    LO_BYTE_16_BIT(callsite),
                },
                {
                    HI_BYTE_16_BIT(driverStatus),
                    LO_BYTE_16_BIT(driverStatus),
                },
                {
                    HI_BYTE_16_BIT(driverReturnValue),
                    LO_BYTE_16_BIT(driverReturnValue),
                },
            };
            memcpy(buffer, &error, sizeof(error));
            dataSize = sizeof(error);
        }
    }
    else if (g_mode == ErrorMode_Cli)
    {
        // @TODO: implement.
    }
    return dataSize;
}


int error_makeUartErrorMessage(uint8_t buffer[], uint16_t size, uint8_t uartStatus, uint16_t callsite)
{
    int dataSize = -1;
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(UartError))
        {
            UartError error =
            {
                ErrorType_Uart,
                uartStatus,
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
        // @TODO: implement.
    }
    return dataSize;
}


int error_makeModeMessage(uint8_t buffer[], uint16_t size)
{
    int dataSize = -1;
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(Mode))
        {
            Mode data =
            {
                ErrorType_Mode,
                g_mode,
            };
            memcpy(buffer, &data, sizeof(data));
            dataSize = sizeof(data);
        }
    }
    else if (g_mode == ErrorMode_Cli)
    {
        // @TODO: implement.
    }
    return dataSize;
}


int error_makeStatsMessage(uint8_t buffer[], uint16_t size)
{
    int dataSize = -1;
    if (g_mode == ErrorMode_Global)
    {
        if (size >= sizeof(Stats))
        {
            Stats data =
            {
                ErrorType_System,
                {
                    HI_BYTE_16_BIT(g_errorCount[ErrorType_System]),
                    LO_BYTE_16_BIT(g_errorCount[ErrorType_System]),
                },
                {
                    HI_BYTE_16_BIT(g_errorCount[ErrorType_Updater]),
                    LO_BYTE_16_BIT(g_errorCount[ErrorType_Updater]),
                },
                {
                    HI_BYTE_16_BIT(g_errorCount[ErrorType_Uart]),
                    LO_BYTE_16_BIT(g_errorCount[ErrorType_Uart]),
                },
                {
                    HI_BYTE_16_BIT(g_errorCount[ErrorType_I2c]),
                    LO_BYTE_16_BIT(g_errorCount[ErrorType_I2c]),
                },
            };
            memcpy(buffer, &data, sizeof(data));
            dataSize = sizeof(data);
        }
    }
    else if (g_mode == ErrorMode_Cli)
    {
        // @TODO: implement.
    }
    return dataSize;
}


/* [] END OF FILE */
