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
    
    /// Process the received data (parse using the UART protocol) and execute
    /// any necessary functions.
    /// @param[in]  data    The received data/buffer to process.
    /// @param[in]  size    The size of the buffer to process.
    /// @return The number of bytes that were processed.
    uint16_t uartFrameProtocol_processRxData(uint8_t const data[], uint16_t size);
    
    /// Processes the pending queue elements in the transmit queue and starts
    /// sending them.
    /// @return The number of bytes that were sent. If 0, then there was nothing
    ///         in the transmit queue to send or nothing could be sent.
    uint16_t uartFrameProtocol_processTxQueue(void);
    
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
