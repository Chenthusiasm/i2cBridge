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

#ifndef I2C_H
    #define I2C_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "heap.h"
    
    
    // === TYPE DEFINES ========================================================
    
    /// Structure that holds the status of I2C functions.
    typedef union I2cStatus
    {        
        /// 8-bit representation of the status. Used to get the bit mask created
        /// by the following anonymous struct of 1-bit flags.
        uint8_t mask;
        
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
        
    } I2cStatus;
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received. Note that if the callback function needs to copy the
    /// received data into its own buffer if the callback needs to perform any
    /// action to the data (like modify the data).
    typedef bool (*I2cRxCallback)(uint8_t const*, uint16_t);
    
    /// Definition of the error callback function that should be invoked when
    /// an error occurs.
    typedef void (*I2cErrorCallback)(I2cStatus, uint16_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the slave I2C hardware.
    void i2c_init(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received.
    /// @param[in]  callback    The callback function.
    void i2c_registerRxCallback(I2cRxCallback callback);
    
    /// Registers the error callback function that should be invoked when an
    /// error occurs.
    /// @param[in]  callback    The callback function.
    void i2c_registerErrorCallback(I2cErrorCallback callback);
    
    /// Registers a new slave address which ensures the write I2C slave device
    /// is addressed when attempting to read when the slaveIRQ line is asserted.
    /// @param[in]  address The new slave address to set.
    void i2c_setSlaveAddress(uint8_t address);
    
    /// Resets the slave address to the default. The slave address is used when
    /// the IRQ line is asserted and a slave read is to be performed.
    void i2c_resetSlaveAddress(void);
    
    /// Accessor to get the driver status mask from the last low-level I2C
    /// driver transaction.
    /// @return The most recent driver status mask.
    uint16_t i2c_getLastDriverStatusMask(void);
    
    /// Accessor to get the driver function return value mask from the last
    /// low-level I2C driver function call.
    /// @return The most recent driver function return value.
    uint16_t i2c_getLastDriverReturnValue(void);
    
    /// Perform an ACK handshake with a specific slave device on the I2C bus.
    /// @param[in]  address The 7-bit I2C address.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait for a default
    ///                     timeout period.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2c_ack(uint8_t address, uint32_t timeoutMs);
    
    /// Perform an ACK handshake with the slave app.
    /// @param[in]  timeout The amount of time in milliseconds the function
    ///                     can wait for the I2C bus to free up before timing
    ///                     out. If 0, then the function will wait for a default
    ///                     timeout period.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2c_ackApp(uint32_t timeoutMs);
    
    /// Perform a blocking read of data from a specific slave device.
    /// @param[in]  address     The 7-bit I2C address.
    /// @param[in]  data        The data buffer to read the data to.
    /// @param[in]  size        The number of bytes to read.
    /// @param[in]  timeoutMs   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then the
    ///                         function will determine the timeout based on
    ///                         the desired bytes to read.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    I2cStatus i2c_read(uint8_t address, uint8_t data[], uint16_t size, uint32_t timeoutMs);
    
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
    ///         I2cStatus union.
    I2cStatus i2c_write(uint8_t address, uint8_t const data[], uint16_t size, uint32_t timeoutMs);
    
    /// Checks the I2cStatus and indicates if any error occurs.
    /// @param[in]  status  The I2cStatus error flags.
    /// @return If an error occurred according to the I2cStatus.
    bool i2c_errorOccurred(I2cStatus const status);
    
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_H


/* [] END OF FILE */
