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

#ifndef I2C_GEN_2_H
    #define I2C_GEN_2_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received. Note that if the callback function needs to copy the
    /// received data into its own buffer if the callback needs to perform any
    /// action to the data (like modify the data).
    typedef uint16_t (*I2CGen2_RxCallback)(uint8_t const*, uint16_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the slave I2C hardware.
    void i2cGen2_init(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received.
    /// @param[in]  callback    Pointer to the callback function.
    void i2cGen2_registerRxCallback(I2CGen2_RxCallback callback);
    
    /// Process any pending data to be receved.
    /// @return The number of bytes that were processed. If -1, an error
    /// occurred: there was data pending but it could not be read.
    int i2cGen2_processRx(void);
    
    /// Process any pending transmits in the transmit queue.
    /// @return The 
    int i2cGen2_processTxQueue(void);
    
    bool i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size);
    
    bool i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size);
    
    bool i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size);
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */
