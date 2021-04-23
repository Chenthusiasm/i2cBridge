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
    
    /// Defines any minor releases that added new features, functions, or bug
    /// fixes; backwards compatibility has been maintained.
    /// Format: unsigned 16-bit integer, represented as decimal.
    #define VERSION_MINOR               (33u)
    
    /// UICO customer number. 999 = internal.
    #define CUSTOMER_NUMBER             (999u)
    
    /// UICO product number.
    #define PRODUCT_NUMBER              (10u)
    
    /// UICO feature number.
    #define FEATURE_NUMBER              (3u)
    
    /// UICO revision number. Show as hex value.
    #define REVISION_NUMBER             (0xa0)
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // VERSION_H


/* [] END OF FILE */
