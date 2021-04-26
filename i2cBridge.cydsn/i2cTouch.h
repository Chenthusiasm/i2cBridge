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
    
    /// Activates the slave I2C module and sets up its globals for touch mode.
    /// This must be invoked before using any read/write/process functions.
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
    
    /// Checks if the module is activated and the heap has been allocated for
    /// touch mode.
    /// @return If the module is activated.
    bool i2cTouch_isActivated(void);
    
    /// Process any pending receive or transmit transactions.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2cTouch_process(uint32_t timeoutMS);
    
    /// Queue up a read from the I2C bus. The i2c_registerRxCallback
    /// function must be invoked with a valid callback function to handle the
    /// received data.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  size    The number of bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2cTouch_read(uint8_t address, uint16_t size);
    
    /// Queue up a write to the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  data    The data buffer to that contains the data to write.
    /// @param[in]  size    The number of bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2cTouch_write(uint8_t address, uint8_t const data[], uint16_t size);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // I2C_TOUCH_H

/* [] END OF FILE */
