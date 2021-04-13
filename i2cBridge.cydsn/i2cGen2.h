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
    typedef union I2cGen2Status
    {        
        /// General flag indicating an error occured; if false, no error
        /// occurred.
        bool errorOccurred;
        
        /// 8-bit representation of the status. Used to get the bit mask created
        /// by the following anonymous struct of 1-bit flags.
        uint8_t value;
        
        /// Anonymous struct of 1-bit flags indicating specific errors.
        struct
        {
            /// Error flag indicating that the module hasn't been activated and
            /// the globals have not had memory dynamically allocated.
            bool deactivated : 1;
            
            /// Error flag indicating a low level driver error occurred.
            bool driverError : 1;
            
            /// Error flag indicating the bus was busy and the I2C transaction
            /// couldn't be completed; the I2C transaction timed out.
            bool timedOut : 1;
            
            /// Error flag indicating a NAK occurred and the slave device could
            /// not be addressed.
            bool nak : 1;
            
            /// Flag indicating that during a read from the I2C slave, invalid
            /// data was received.
            bool invalidRead : 1;
            
            /// Error flag indicating that the transmit queue is full.
            bool transmitQueueFull : 1;
            
            /// Error flag indicating that the input parameters are invalid.
            bool inputParametersInvalid : 1;
        };
        
    } I2cGen2Status;
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received. Note that if the callback function needs to copy the
    /// received data into its own buffer if the callback needs to perform any
    /// action to the data (like modify the data).
    typedef bool (*I2cGen2_RxCallback)(uint8_t const*, uint16_t);
    
    /// Definition of the error callback function that should be invoked when
    /// an error occurs.
    typedef void (*I2cGen2_ErrorCallback)(I2cGen2Status, uint16_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the slave I2C hardware.
    void i2cGen2_init(void);
    
    /// Accessor to get the number of bytes required for global variables.
    /// @return The number of bytes need for global variables.
    uint16_t i2cGen2_getMemoryRequirement(void);
    
    /// Activates the slave I2C gen 2 module and sets up its globals. This must
    /// be invoked before using any processRx or processTx-like functions.
    /// @param[in]  memory  Memory buffer that is available for the module's
    ///                     globals. This is an array of 32-bit words to help
    ///                     preserve word alignment.
    /// @param[in]  size    The size (in 32-bit words) of the memory array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t i2cGen2_activate(uint32_t memory[], uint16_t size);
    
    /// Deactivates the slave I2C module and effectively deallocates the memory.
    void i2cGen2_deactivate(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received.
    /// @param[in]  callback    The callback function.
    void i2cGen2_registerRxCallback(I2cGen2_RxCallback callback);
    
    /// Registers the error callback function that should be invoked when an
    /// error occurs.
    /// @param[in]  callback    The callback function.
    void i2cGen2_registerErrorCallback(I2cGen2_ErrorCallback callback);
    
    /// Registers a new slave address which ensures the write I2C slave device
    /// is addressed when attempting to read when the slaveIRQ line is asserted.
    /// @param[in]  address The new slave address to set.
    void i2cGen2_setSlaveAddress(uint8_t address);
    
    /// Resets the slave address to the default. The slave address is used when
    /// the IRQ line is asserted and a slave read is to be performed.
    void i2cGen2_resetSlaveAddress(void);
    
    /// Accessor to get the driver status mask from the last low-level I2C
    /// driver transaction.
    /// @return The most recent driver status mask.
    uint16_t i2cGen2_getLastDriverStatusMask(void);
    
    /// Accessor to get the driver function return value mask from the last
    /// low-level I2C driver function call.
    /// @return The most recent driver function return value.
    uint16_t i2cGen2_getLastDriverReturnValue(void);
    
    /// Process any pending data to be received.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return If pending data was successfully received and processed. If
    ///         nothing is pending, the function will also return false.
    bool i2cGen2_processRx(uint32_t timeoutMS);
    
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
    
    /// Read from the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  data    The data buffer to store the read data.
    /// @param[in]  size    The number of bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  data    The data buffer to that contains the data to write.
    /// @param[in]  size    The number of bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Write to the I2C bus; the I2C address in in the data buffer.
    /// @param[in]  data    The data buffer to that contains the data to write.
    ///                     The first byte is assumed to be the I2C address.
    /// @param[in]  size    The number of bytes in data (including the I2C
    ///                     address).
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size);
    
    /// Enqueue a transmit packet.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  data    The data buffer to that contains the data to
    ///                     transmit.
    /// @param[in]  size    The number of bytes in data.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size);
    
    /// Enqueue a transmit packet. The 7-bit I2C address is the first byte in
    /// the data buffer.
    /// @param[in]  data    The data buffer to that contains the data to
    ///                     transmit. The first byte is the I2C address.
    /// @param[in]  size    The number of bytes in data.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size);
    
    /// Perform an ACK handshake with a specific slave device on the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait indefinitely
    ///                     until the bus is free.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_ack(uint8_t address, uint32_t timeoutMS);
    
    /// Perform an ACK handshake with the slave app.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait indefinitely
    ///                     until the bus is free.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    I2cGen2Status i2cGen2_ackApp(uint32_t timeoutMS);
    
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */
