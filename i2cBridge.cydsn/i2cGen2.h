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
    
    /// Structure that holds the status of I2C gen 2 functions.
    typedef union I2CGen2Status_
    {
        struct
        {
            /// Error flag indicating that the system itself is hasn't been
            /// started.
            bool systemNotStarted : 1;
            
            /// Error flag indicating a low level driver error occurred.
            bool driverError : 1;
            
            /// Error flag indicating the bus was busy and the I2C transaction
            /// couldn't be completed; a timeout occurred.
            bool busBusy : 1;
            
            /// Error flag indicating a NAK occurred and the slave device could
            /// not be addressed.
            bool nak : 1;
            
            /// Error flag indicating that the transmit queue is full.
            bool transmitQueueFull : 1;
            
            /// Error flag indicating that the input parameters are invalid.
            bool inputParametersInvalid : 1;
        };
        
        /// General flag indicating an error occured; if false, no error
        /// occurred.
        bool errorOccurred;
        
    } I2CGen2Status;
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received. Note that if the callback function needs to copy the
    /// received data into its own buffer if the callback needs to perform any
    /// action to the data (like modify the data).
    typedef bool (*I2CGen2_RxCallback)(uint8_t const*, uint16_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the slave I2C hardware.
    void i2cGen2_init(void);
    
    /// Starts the slave I2C gen 2 system and sets up its memory buffer. This
    /// must be invoked before using any processRx or processTx-like functions.
    /// @param[in]  memory  Memory location
    /// @param[in]  size    The size available to the 
    /// @return The number of bytes that the start function allocated from
    ///         memory for its own purpose. If 0, then the memory was not able
    ///         to be allocated and the system didn't start.
    uint16_t i2cGen2_start(uint8_t* memory, uint16_t size);
    
    /// Stops the slave I2C system and effectively deallocates the memory.
    void i2cGen2_stop(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received.
    /// @param[in]  callback    Pointer to the callback function.
    void i2cGen2_registerRxCallback(I2CGen2_RxCallback callback);
    
    /// Registers a new slave address which ensures the write I2C slave device
    /// is addressed when attempting to read when the slaveIRQ line is asserted.
    /// @param[in]  address The new slave address to set.
    void i2cGen2_setSlaveAddress(uint8_t address);
    
    /// Resets the slave address to the default. The slave address is used when
    /// the IRQ line is asserted and a slave read is to be performed.
    void i2cGen2_resetSlaveAddress(void);
    
    /// Process any pending data to be receved.
    /// @return The number of bytes that were processed. If 0, then no bytes
    ///         were pending to receive. If -1, an error occurred: there was
    ///         data pending but it could not be read because the bus was busy.
    int i2cGen2_processRx(void);
    
    /// Process any pending transmits in the transmits.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @param[in]  quitIfBusy  Flag indicating if the function should quit if
    ///                         the bus is busy and not timeout.
    /// @return The number of packets that were transmitted. If 0, then no
    ///         packets are pending to transmit. If -1, a timeout occurred or if
    ///         the quitIfBusy flag is set, then the bus was busy so the
    ///         function ended early.
    int i2cGen2_processTxQueue(uint32_t timeoutMS, bool quitIfBusy);
    
    /// Read from the i2C bus.
    /// @param[in]  address The I2C address.
    /// @param[in]  data    The data buffer to store the read data.
    /// @param[in]  size    The number of bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the i2C bus.
    /// @param[in]  address The I2C address.
    /// @param[in]  data    The data buffer to that contains the data to write.
    /// @param[in]  size    The number of bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the i2C bus; the I2C address in in the data buffer.
    /// @param[in]  data    The data buffer to that contains the data to write.
    ///                     The first byte is assumed to be the I2C address.
    /// @param[in]  size    The number of bytes in data (including the I2C
    ///                     address).
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size);
    
    /// Enqueue a transmit packet.
    /// @param[in]  address The I2C address.
    /// @param[in]  data    The data buffer to that contains the data to
    ///                     transmit.
    /// @param[in]  size    The number of bytes in data.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Enqueue a transmit packet. The I2C address is the first byte in the data
    /// buffer.
    /// @param[in]  data    The data buffer to that contains the data to
    ///                     transmit. The first byte is the I2C address.
    /// @param[in]  size    The number of bytes in data.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size);
    
    /// Perform an ACK handshake with the slave app.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait indefinitely
    ///                     until the bus is free.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2CGen2Status union.
    I2CGen2Status i2cGen2_appACK(uint32_t timeoutMS);
    
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */
