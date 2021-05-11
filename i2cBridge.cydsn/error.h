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
    
    #include "bridgeFsm.h"
    #include "i2c.h"
    #include "uartUpdate.h"
    
    
    // === TYPE DEFINES ========================================================
    
    /// Type define for the callsite variable used for error messages. The
    /// callsite is a unique ID used to help identify where an error may have
    /// occurred. Use the following recommendations when defining unique
    /// callsite IDs.
    /// 1.  Create a static global variable (file-scope) that contains the
    ///     current callsite. This will help when drilling to specific private
    ///     function invocations that may have triggered an error.
    /// 2.  Public functions in a module should use the most significant byte to
    ///     define the function callsite (0xff00).
    /// 3.  Any private function should use the least significant byte to define
    ///     an increment value to the callsite; this way, the public function's
    ///     callsite will be retained (0x00ff).
    /// 4.  The private function mask can be further split up into upper and
    ///     lower nibbles where the upper nibble can be reserved specific blocks
    ///     of code executed in the public function or a called inner function.
    ///     The low nibble can be reserved for specific private functions or
    ///     blocks of code that are repeatedly called by different functions.
    ///     This way, if both types of functions are called when the error
    ///     occurred, they don't mask each other off.
    /// 5.  Function calls that are mutually exclusive can be defined in the
    ///     entire range of the decribed masks above (i.e. public functions that
    ///     use the 0xff00 mask has a range of 256 instead of 8 in the 8-bit
    ///     mask).
    typedef uint16_t callsite_t;
    
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
        
        /// Error in the update function.
        ErrorType_Update,
        
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
    /// @param[out] buffer      The buffer to put the I2C error message.
    /// @param[in]  size        The size of the buffer.
    /// @param[in]  status      The system status.
    /// @param[in]  callsite    The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeSystemErrorMessage(uint8_t buffer[], uint16_t size, BridgeStatus status, callsite_t callsite);
    
    /// Generates the system error message.
    /// @param[out] buffer      The buffer to put the I2C error message.
    /// @param[in]  size        The size of the buffer.
    /// @param[in]  status      The update status.
    /// @param[in]  callsite    The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeUpdateErrorMessage(uint8_t buffer[], uint16_t size, UpdateStatus status, callsite_t callsite);
    
    /// Generates the I2C error message.
    /// @param[out] buffer      The buffer to put the I2C error message.
    /// @param[in]  size        The size of the buffer.
    /// @param[in]  status      The I2C status.
    /// @param[in]  callsite    The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeI2cErrorMessage(uint8_t buffer[], uint16_t size, I2cStatus status, callsite_t callsite);
    
    /// Generates the UART error message.
    /// @param[out] buffer      The buffer to put the I2C error message.
    /// @param[in]  size        The size of the buffer.
    /// @param[in]  status      The UART status.
    /// @param[in]  callsite    The callsite ID.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeUartErrorMessage(uint8_t buffer[], uint16_t size, uint8_t status, callsite_t callsite);
    
    /// Generates the error mode message.
    /// @param[out] buffer  The buffer to put the I2C error message.
    /// @param[in]  size    The size of the buffer.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeModeMessage(uint8_t buffer[], uint16_t size);
    
    /// Generates the error stats message.
    /// @param[out] buffer  The buffer to put the I2C error message.
    /// @param[in]  size    The size of the buffer.
    /// @return The size of the data payload. If -1, the buffer size was too
    ///         small.
    int error_makeStatMessage(uint8_t buffer[], uint16_t size);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // ERROR_H

/* [] END OF FILE */
