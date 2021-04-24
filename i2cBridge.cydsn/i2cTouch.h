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

#ifndef I2C_TOUCH_H
    #define I2C_TOUCH_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "i2c.h"
    
    
    // === TYPE DEFINES ========================================================
    
    
    // === FUNCTIONS ===========================================================
    
    /// Accessor to get the number of heap words required for global variables.
    /// @return The number of heap words needed for global variables.
    uint16_t i2cTouch_getHeapWordRequirement(void);
    
    /// Activates the slave I2C gen 2 module and sets up its globals. This must
    /// be invoked before using any processRx or processTx-like functions.
    /// @param[in]  memory  Memory buffer that is available for the module's
    ///                     globals. This is an array of 32-bit words to help
    ///                     preserve word alignment.
    /// @param[in]  size    The size (in 32-bit words) of the memory array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t i2cTouch_activate(heapWord_t memory[], uint16_t size);
    
    /// Deactivates the slave I2C module and effectively deallocates the memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t i2cTouch_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated.
    /// @return If the module is activated.
    bool i2cTouch_isActivated(void);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // I2C_TOUCH_H

/* [] END OF FILE */
