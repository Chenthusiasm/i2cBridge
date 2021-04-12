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
    #define VERSION_UPDATE              (13u)
    
    /// Defines the legacy variation of the version; major release. The legacy
    /// major version is hard-coded to 254 (0xfe).
    /// DO NOT MODIFY.
    #define VERSION_LEGACY_MAJOR        (0xfe)
    
    /// Defines the legacy variation of the version; minor release. The le
    /// DO NOT MODIFY.
    #define VERSION_LEGACY_MINOR        (VERSION_UPDATE)
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // VERSION_H


/* [] END OF FILE */
