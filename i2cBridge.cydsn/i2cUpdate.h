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

#ifndef I2C_UPDATE_H
    #define I2C_UPDATE_H

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
    uint16_t i2cUpdate_getHeapWordRequirement(void);
    
    /// Activates the slave I2C module and sets up its globals for update mode.
    /// This must be invoked before using any read/write/process functions.
    /// @param[in]  memory  Memory buffer that is available for the module's
    ///                     globals. This is an array of 32-bit words to help
    ///                     preserve word alignment.
    /// @param[in]  size    The size (in 32-bit words) of the memory array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t i2cUpdate_activate(heapWord_t memory[], uint16_t size);
    
    /// Deactivates the slave I2C module and effectively deallocates the memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t i2cUpdate_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// update mode.
    /// @return If the module is activated.
    bool i2cUpdate_isActivated(void);
    
    /// Perform a blocking read of data from a specific slave device.
    /// @param[in]  address     The 7-bit I2C address.
    /// @param[in]  data        The data buffer to read the data to.
    /// @param[in]  size        The number of bytes to read.
    /// @param[in]  timeoutMs   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then the
    ///                         function will determine the timeout based on
    ///                         the desired bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus structure.
    I2cStatus i2cUpdate_read(uint8_t address, uint8_t data[], uint16_t size, uint32_t timeoutMs);
    
    /// Perform a blocking write of data to a specific slave device.
    /// @param[in]  address     The 7-bit I2C address.
    /// @param[in]  data        The data buffer to that contains the data to
    ///                         write.
    /// @param[in]  size        The number of bytes to write.
    /// @param[in]  timeoutMs   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then the
    ///                         function will determine the timeout based on
    ///                         the desired bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus structure.
    I2cStatus i2cUpdate_write(uint8_t address, uint8_t const data[], uint16_t size, uint32_t timeoutMs);
    
    /// Perform a blocking read of data to the bootloader slave device.
    /// @param[in]  data        The data buffer to read the data to.
    /// @param[in]  size        The number of bytes to read.
    /// @param[in]  timeoutMs   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then the
    ///                         function will determine the timeout based on
    ///                         the desired bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus structure.
    I2cStatus i2cUpdate_bootloaderRead(uint8_t data[], uint16_t size, uint32_t timeoutMs);
    
    /// Perform a blocking write of data to the bootloader slave device.
    /// @param[in]  data        The data buffer to that contains the data to
    ///                         write.
    /// @param[in]  size        The number of bytes to write.
    /// @param[in]  timeoutMs   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then the
    ///                         function will determine the timeout based on
    ///                         the desired bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus structure.
    I2cStatus i2cUpdate_bootloaderWrite(uint8_t const data[], uint16_t size, uint32_t timeoutMs);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // I2C_UPDATE_H

/* [] END OF FILE */
