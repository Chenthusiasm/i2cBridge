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
    
    #include "heap.h"
    
    
    // === TYPE DEFINES ========================================================
    
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
    
    /// Accessor to get the number of heap words required for global variables.
    /// @param[in]  enableUpdater   Flag indicating if updater mode is enabled,
    ///                             otherwise normal mode is enabled.
    /// @return The number of heap words needed for global variables.
    uint16_t uartFrameProtocol_getHeapWordRequirement(bool enableUpdater);
    
    /// Activates the UART frame protocol module and sets up its globals.
    /// This must be invoked before using any processRx or processTx-like
    /// functions.
    /// @param[in]  memory          Memory buffer that is available for the
    ///                             module's globals. This is an array of 32-bit
    ///                             worlds to help preserve word alignment.
    /// @param[in]  size            The size (in 32-bit words) of the memory
    ///                             array.
    /// @param[in]  enableUpdater   Flag indicating if updater mode is enabled,
    ///                             otherwise normal mode is enabled.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t uartFrameProtocol_activate(heapWord_t memory[], uint16_t size, bool enableUpdater);
    
    /// Deactivates the UART frame protocol module and effectively deallocates
    /// the global memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t uartFrameProtocol_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// normal mode.
    /// @return If normal mode is activated.
    bool uartFrameProtocol_isActivated(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// updater mode.
    /// @return If normal mode is activated.
    bool uartFrameProtocol_isUpdaterActivated(void);
    
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
    
    /// Enqueue error with associated data into the transmit queue.
    /// @param[in]  data    The data associated with the error to enqueue.
    /// @param[in]  size    The size of the data.
    /// @return If the data was successfully enqueued.
    bool uartFrameProtocol_txEnqueueError(uint8_t const data[], uint16_t size);
    
    /// Directly write a string to the UART. The write only occurs if the
    /// the module is deactivated; use the txEnqueData and processTx if the
    /// module is activated.
    /// @param[in]  string  The string to write to the UART.
    void uartFrameProtocol_write(char const string[]);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_FRAME_PROTOCOL_H


/* [] END OF FILE */
