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

#ifndef ERROR_H
    #define ERROR_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "bridgeStateMachine.h"
    #include "i2cGen2.h"
    
    
    // === TYPE DEFINES ========================================================
    
    /// Enumerations for the different types of alarms.
    typedef enum ErrorMode
    {
        /// The legacy error mode where errors are separated out in different
        /// UART frame protocol commands.
        ErrorMode_Legacy,
        
        /// Global error mode.
        ErrorMode_Global,
        
        /// Command line interface (CLI) mode.
        ErrorMode_Cli,
        
    } ErrorMode;
    
    
    /// Enumeration that defines the different error type messages.
    typedef enum ErrorType
    {
        /// Overall system-level error in the bridge.
        ErrorType_System,
        
        /// Error in the updater function.
        ErrorType_Updater,
        
        /// Error in the UART.
        ErrorType_Uart,
        
        /// Error in the I2C interface.
        ErrorType_I2c,
        
        /// Mode of the error system.
        ErrorType_Mode,
        
        /// Statistics of the global error reporting.
        ErrorType_Stats,
        
    } ErrorType;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Accessor to get the current error mode.
    /// @return The current error mode.
    ErrorMode error_getMode(void);
    
    /// Accessor to set the current error mode.
    /// @param[in]  mode    The error mode to set.
    void error_setMode(ErrorMode mode);
    
    /// Tally/count the error for tracking purposes.
    /// @param[in]  type    The error type to tally.
    void error_tally(ErrorType type);
    
    /// Generates the system error message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @param[in]  systemStatus    The system status.
    /// @param[in]  callsite        The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeSystemErrorMessage(uint8_t buffer[], uint16_t size, SystemStatus systemStatus, uint16_t callsite);
    
    /// Generates the system error message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @param[in]  updaterStatus   The updater status.
    /// @param[in]  callsite        The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeUpdaterErrorMessage(uint8_t buffer[], uint16_t size, uint8_t updaterStatus, uint16_t callsite);
    
    /// Generates the I2C error message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @param[in]  i2cStatus       The I2C status.
    /// @param[in]  callsite        The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeI2cErrorMessage(uint8_t buffer[], uint16_t size, I2cGen2Status i2cStatus, uint16_t callsite);
    
    /// Generates the UART error message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @param[in]  uartStatus      The UART status.
    /// @param[in]  callsite        The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeUartErrorMessage(uint8_t buffer[], uint16_t size, uint8_t uartStatus, uint16_t callsite);
    
    /// Generates the error mode message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeModeMessage(uint8_t buffer[], uint16_t size);
    
    /// Generates the error stats message.
    /// @param[out] buffer          The buffer to put the I2C error message.
    /// @param[in]  size            The size of the buffer.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeStatMessage(uint8_t buffer[], uint16_t size);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // ERROR_H

/* [] END OF FILE */
