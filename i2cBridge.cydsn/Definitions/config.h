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

#ifndef CONFIG_PROJECT_H
    #define CONFIG_PROJECT_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === DEFINES: DEBUG ======================================================
    
    /// Enable the serial wire debug (SWD) functionality. Note that the SWD SDAT
    /// and SCLK lines are mapped to the same pins as debug pin 0, debug pin 1,
    /// and the debug UART transmit. SWD takes priority over all these other
    /// debug functions so if SWD is enabled, it will be on over these other
    /// debug functions.
    #define ENABLE_DEBUG_SWD                                (false)
    
    /// Enable the debug pin 0 functionality.
    #define ENABLE_DEBUG_PIN_0                              (true)
    
    /// Enable the debug pin 1 functionality.  Debug pin 1 is mapped to the same
    /// GPIO as the debug UART transmit pin. The debug UART takes priority so if
    /// both are enabled, the debug UART will be on by default.
    #define ENABLE_DEBUG_PIN_1                              (true)
    
    /// Enable the debug UART functionality. Note the debug UART is transmit-
    /// only and the transmit pin is mapped to the same GPIO as debug pin 1. The
    /// debug UART takes priority so if both are enabled, the debug UART will
    /// be on by default.
    #define ENABLE_DEBUG_UART                               (true)
    
    
    // === DEFINES: I2C ========================================================
    
    /// Enable/disable the locked I2C bus detection and recovery.
    #define ENABLE_I2C_LOCKED_BUS_DETECTION                 (true)
    
    
    // === DEFINES: UART =======================================================
    
    
    // === DEFINES: PRINTF =====================================================
    
    /// Enable the optimized decimal divide and modulo; this doesn't use the
    /// standard divide and modulo operators.
    #define ENABLE_PRINTF_FAST_DIVIDE_BY_10                 (true)

    /// Enable the binary (base 2) formatter.
    #define ENABLE_PRINTF_BINARY                            (false)

    /// Enable the octal (base 8) formatter.
    #define ENABLE_PRINTF_OCTAL                             (false)

    /// Enable the hexadecimal (base 16) formatter.
    #define ENABLE_PRINTF_HEX                               (true)
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // CONFIG_PROJECT_H


/* [] END OF FILE */
