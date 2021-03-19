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

#ifndef HW_SYSTEM_TIME_H
    #define HW_SYSTEM_TIME_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the system timer.
    /// @param[in]  periodMS    The time period in milliseconds at which the
    ///                         system time will be tracked.
    void hwSystemTime_init(uint16_t periodMS);
    
    /// Gets the current value of the system time in milliseconds.
    /// @return The current system time in milliseconds.
    uint32_t hwSystemTime_getCurrentMS(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // HW_SYSTEM_TIME_H


/* [] END OF FILE */
