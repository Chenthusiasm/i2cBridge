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

#ifndef UART_FRAME_PROTOCOL_H
    #define UART_FRAME_PROTOCOL_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPEDEFINES =========================================================
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received out of frame (therefore, don't process it in the
    /// context of the framing).  Note that the callback function should copy
    /// the received data into its own buffer.
    typedef bool (*UartFrameProtocol_RxOutOfFrameCallback)(uint8_t);
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received but the receive buffer has overflowed. Perform some
    /// error handling/messaging in the callback function.
    typedef bool (*UartFrameProtocol_RxFrameOverflowCallback)(uint8_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the communications interface.
    void uartFrameProtocol_init(void);
    
    /// Starts the UART frame protocl system and sets up its memory buffer. This
    /// must be invoked before using any processRx or processTx-like functions.
    /// @param[in]  memory  Memory location
    /// @param[in]  size    The size available to the 
    /// @return The number of bytes that the start function allocated from
    ///         memory for its own purpose. If 0, then the memory was not able
    ///         to be allocated and the system didn't start.
    uint16_t uartFrameProtocol_start(uint8_t* memory, uint16_t size);
    
    /// Stops the slave I2C system and effectively deallocates the memory.
    void uartFrameProtocol_stop(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received out of frame.
    /// @param[in]  callback    Pointer to the callback function.
    void uartFrameProtocol_registerRxOutOfFrameCallback(UartFrameProtocol_RxOutOfFrameCallback callback);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received but it cannot be put int the receive buffer due to data
    /// overflow.
    /// @param[in]  callback    Pointer to the callback function.
    void uartFrameProtocol_registerRxFrameOverflowCallback(UartFrameProtocol_RxFrameOverflowCallback callback);
    
    /// Checks to see if the transmit queue is empty. If the transmit queue is
    /// empty, there is nothing to send.
    /// @return If the transmit queue is empty.
    bool uartFrameProtocol_isTxQueueEmpty(void);
    
    /// Processes any pending receives and executes any functions associated
    /// with received UART packets.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return The number of packets that were processed.
    uint16_t uartFrameProtocol_processRx(uint32_t timeoutMS);
    
    /// Processes any pending transmits and attempts to transmit any UART
    /// packets waiting to be sent.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return The number of packets that were processed.
    uint16_t uartFrameProtocol_processTx(uint32_t timeoutMS);
    
    /// Enqueue data into the transmit queue.
    /// @param[in]  data    The data to enqueue.
    /// @param[in]  size    The size of the data.
    /// @return If the data was successfully enqueued.
    bool uartFrameProtocol_txEnqueueData(uint8_t const data[], uint16_t size);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_FRAME_PROTOCOL_H


/* [] END OF FILE */
