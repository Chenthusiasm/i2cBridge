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
    ///         occurred: there was data pending but it could not be read.
    int i2cGen2_processRx(void);
    
    /// Process any pending transmits in the transmits.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return The number of packets that were transmitted. If -1, an error
    ///         occurred: there was packets pending but it could not be sent.
    int i2cGen2_processTx(uint32_t timeoutMS);
    
    /// Read from the i2C bus.
    /// @param[in]  address The I2C address.
    /// @param[in]  data    The data buffer to store the read data.
    /// @param[in]  size    The number of bytes to read.
    /// @return If the read was successful. If not, then the I2C bus was busy.
    bool i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the i2C bus.
    /// @param[in]  address The I2C address.
    /// @param[in]  data    The data buffer to that contains the data to write.
    /// @param[in]  size    The number of bytes to write.
    /// @return If the write was successful. If not, then the I2C bus was busy.
    bool i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the i2C bus; the I2C address in in the data buffer.
    /// @param[in]  data    The data buffer to that contains the data to write.
    ///                     The first byte is assumed to be the I2C address.
    /// @param[in]  size    The number of bytes in data (including the I2C
    ///                     address).
    /// @return If the write was successful. If not, then the I2C bus was busy.
    bool i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size);
    
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */
