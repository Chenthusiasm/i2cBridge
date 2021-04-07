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

#ifndef VERSION_H
    #define VERSION_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === DEFINES: DEBUG ======================================================
    
    /// Defines a major release that breaks backwards compatibility for previous
    /// versions when interfacing with the external world (I2C, UART, and other
    /// external communication buses).
    /// Additionally, development builds that have not been released outside of
    /// the development team should set this value to 0.
    /// Format: unsigned 16-bit integer, represented as decimal.
    #define VERSION_MAJOR               (0u)
    
    /// Defines any minor releases that added new features, functions, or
    /// significant bug fixes but backwards compatibility has been maintained.
    /// Format: unsigned 16-bit integer, represented as decimal.
    #define VERSION_MINOR               (0u)
    
    /// Defines any small bug fixes or code rework.
    /// Format: unsigned 16-bit integer, represented as decimal.
    #define VERSION_REVISION            (3u)
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // VERSION_H


/* [] END OF FILE */
