/* ========================================
 *
 * UICO, 2021
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * Refer to CE224703: PSoC4 Watchdog Timer
 * example project for reference.
 *
 * ========================================
*/

#ifndef HW_WATCHDOG_H
    #define HW_WATCHDOG_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the watchdog, sets up the watchdog timeout, and starts the
    /// watchdog.
    /// @param[in]  timeoutMS   The timeout period in milliseconds. If the
    ///                         timeout expires, the watchdog will trigger a
    ///                         system reset.
    void hwWatchdog_init(uint16_t timeoutMS);
    
    /// Accessor that checks if the watchdog is running (started).
    /// @return If the watchdog is running (started).
    bool hwWatchdog_isRunning(void);
    
    /// Start the watchdog. The watchdog must be fed before the timeout occurs
    /// otherwise a system reset will occur.
    void hwWatchdog_start(void);
    
    /// Stop the watchdog.
    void hwWatchdog_stop(void);
    
    /// Feed the watchdog to reset the timeout and prevent a reset. Only invoke
    /// this function in a processing loop running from the context of main. Do
    /// not invoke this in an ISR.
    void hwWatchdog_feed(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // HW_WATCHDOG_H

/* [] END OF FILE */
