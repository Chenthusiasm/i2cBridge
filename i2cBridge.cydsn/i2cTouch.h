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
    typedef union I2cTouchStatus
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
            
            /// Error flag indicating that the input parameters are invalid.
            bool invalidInputParameters : 1;
            
            /// Error flag indicating a low level driver error occurred.
            bool driverError : 1;
            
            /// Error flag indicating the I2C bus appears in a locked-up/stuck
            /// state and no low-level I2C functions have been able to be run.
            bool lockedBus : 1;
            
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
            bool queueFull : 1;
            
            
        };
        
    } I2cTouchStatus;
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received. Note that if the callback function needs to copy the
    /// received data into its own buffer if the callback needs to perform any
    /// action to the data (like modify the data).
    typedef bool (*I2cTouch_RxCallback)(uint8_t const*, uint16_t);
    
    /// Definition of the error callback function that should be invoked when
    /// an error occurs.
    typedef void (*I2cTouch_ErrorCallback)(I2cTouchStatus, uint16_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the slave I2C hardware.
    void i2cTouch_init(void);
    
    /// Accessor to get the number of bytes required for global variables.
    /// @return The number of bytes need for global variables.
    uint16_t i2cTouch_getMemoryRequirement(void);
    
    /// Activates the slave I2C gen 2 module and sets up its globals. This must
    /// be invoked before using any processRx or processTx-like functions.
    /// @param[in]  memory  Memory buffer that is available for the module's
    ///                     globals. This is an array of 32-bit words to help
    ///                     preserve word alignment.
    /// @param[in]  size    The size (in 32-bit words) of the memory array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t i2cTouch_activate(uint32_t memory[], uint16_t size);
    
    /// Deactivates the slave I2C module and effectively deallocates the memory.
    void i2cTouch_deactivate(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received.
    /// @param[in]  callback    The callback function.
    void i2cTouch_registerRxCallback(I2cTouch_RxCallback callback);
    
    /// Registers the error callback function that should be invoked when an
    /// error occurs.
    /// @param[in]  callback    The callback function.
    void i2cTouch_registerErrorCallback(I2cTouch_ErrorCallback callback);
    
    /// Registers a new slave address which ensures the write I2C slave device
    /// is addressed when attempting to read when the slaveIRQ line is asserted.
    /// @param[in]  address The new slave address to set.
    void i2cTouch_setSlaveAddress(uint8_t address);
    
    /// Resets the slave address to the default. The slave address is used when
    /// the IRQ line is asserted and a slave read is to be performed.
    void i2cTouch_resetSlaveAddress(void);
    
    /// Accessor to get the driver status mask from the last low-level I2C
    /// driver transaction.
    /// @return The most recent driver status mask.
    uint16_t i2cTouch_getLastDriverStatusMask(void);
    
    /// Accessor to get the driver function return value mask from the last
    /// low-level I2C driver function call.
    /// @return The most recent driver function return value.
    uint16_t i2cTouch_getLastDriverReturnValue(void);
    
    /// Process any pending receive or transmit transactions.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cTouchStatus union.
    I2cTouchStatus i2cTouch_process(uint32_t timeoutMS);
    
    /// Queue up a read from the I2C bus. The i2cTouch_registerRxCallback
    /// function must be invoked with a valid callback function to handle the
    /// received data.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  size    The number of bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cTouchStatus union.
    I2cTouchStatus i2cTouch_read(uint8_t address, uint16_t size);
    
    /// Queue up a write to the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  data    The data buffer to that contains the data to write.
    /// @param[in]  size    The number of bytes to write.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cTouchStatus union.
    I2cTouchStatus i2cTouch_write(uint8_t address, uint8_t const data[], uint16_t size);
    
    /// Perform an ACK handshake with a specific slave device on the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait for a default
    ///                     timeout period.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cTouchStatus union.
    I2cTouchStatus i2cTouch_ack(uint8_t address, uint32_t timeoutMS);
    
    /// Perform an ACK handshake with the slave app.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait for a default
    ///                     timeout period.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cTouchStatus union.
    I2cTouchStatus i2cTouch_ackApp(uint32_t timeoutMS);
    
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */