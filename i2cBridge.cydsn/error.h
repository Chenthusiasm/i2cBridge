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
    
    
    // === TYPE DEFINES ========================================================
    
    /// Enumerations for the different types of alarms.
    typedef enum ErrorMode
    {
        /// The legacy error mode where errors are separated out in different
        /// UART frame protocol commands.
        ErrorMode_Legacy,
        
        /// Global error mode.
        ErrorMode_Global,
        
    } ErrorMode;
    
    
    /// Enumeration that defines the different error type messages.
    typedef enum ErrorType
    {
        /// Status of the global error reporting.
        ErrorType_Status                    = 0u,
        
        /// Overall system-level error in the bridge.
        ErrorType_System                    = 1u,
        
        /// Error in the updater function.
        ErrorType_Updater                   = 2u,
        
        /// Error in the UART.
        ErrorType_Uart                      = 3u,
        
        /// Error in the I2C interface.
        ErrorType_I2c                       = 4u,
        
    } ErrorType;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Accessor to get the current error mode.
    /// @return The current error mode.
    ErrorMode error_getMode(void);
    
    /// Accessor to set the current error mode.
    /// @param[in]  mode    The error mode to set.
    void error_setMode(ErrorMode mode);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // ERROR_H

/* [] END OF FILE */
