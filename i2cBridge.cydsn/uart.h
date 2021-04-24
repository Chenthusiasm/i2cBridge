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
#ifndef UART_H
    #define UART_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "heap.h"
    #include "queue.h"
    
    
    // === TYPE DEFINES ========================================================
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received out of frame (therefore, don't process it in the
    /// context of the framing).  Note that the callback function should copy
    /// the received data into its own buffer.
    typedef bool (*UartRxOutOfFrameCallback)(uint8_t);
    
    /// Definition of the receive callback function that should be invoked when
    /// data is received but the receive buffer has overflowed. Perform some
    /// error handling/messaging in the callback function.
    typedef bool (*UartRxFrameOverflowCallback)(uint8_t);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the communications interface.
    void uart_init(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received out of frame.
    /// @param[in]  callback    Pointer to the callback function.
    void uart_registerRxOutOfFrameCallback(UartRxOutOfFrameCallback callback);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received but it cannot be put int the receive buffer due to data
    /// overflow.
    /// @param[in]  callback    Pointer to the callback function.
    void uart_registerRxFrameOverflowCallback(UartRxFrameOverflowCallback callback);
    
    /// Checks to see if the transmit queue is empty. If the transmit queue is
    /// empty, there is nothing to send.
    /// @return If the transmit queue is empty.
    bool uart_isTxQueueEmpty(void);
    
    /// Processes any pending receives and executes any functions associated
    /// with received UART packets.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return The number of packets that were processed.
    uint16_t uart_processRx(uint32_t timeoutMS);
    
    /// Processes any pending transmits and attempts to transmit any UART
    /// packets waiting to be sent.
    /// @param[in]  timeoutMS   The amount of time the process can occur before
    ///                         it times out and must finish. If 0, then there's
    ///                         no timeout and the function blocks until all
    ///                         pending actions are completed.
    /// @return The number of packets that were processed.
    uint16_t uart_processTx(uint32_t timeoutMS);
    
    /// Enqueue data into the transmit queue.
    /// @param[in]  data    The data to enqueue.
    /// @param[in]  size    The size of the data.
    /// @return If the data was successfully enqueued.
    bool uart_txEnqueueData(uint8_t const data[], uint16_t size);
    
    /// Enqueue error with associated data into the transmit queue.
    /// @param[in]  data    The data associated with the error to enqueue.
    /// @param[in]  size    The size of the data.
    /// @return If the data was successfully enqueued.
    bool uart_txEnqueueError(uint8_t const data[], uint16_t size);
    
    /// Directly write a string to the UART. The write only occurs if the
    /// the module is deactivated; use the txEnqueData and processTx if the
    /// module is activated.
    /// @param[in]  string  The string to write to the UART.
    void uart_write(char const string[]);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_H


/* [] END OF FILE */
