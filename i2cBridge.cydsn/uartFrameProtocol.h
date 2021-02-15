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
    
    /// Type define for the receive callback function that should be invoked
    /// when data is received out of frame (therefore, don't process it in the
    /// context of the framing).  Note that the callback function should copy
    /// the received data into its own buffer.
    typedef bool (*UartFrameProtocol_RxOutOfFrameCallback)(uint8_t);
    
    /// Type define for the receive callback function that should be invoked
    /// when data is received but the receive buffer has overflowed. Perform
    /// some error handling/messaging in the callback function.
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
    
    /// Process the received data (parse using the UART protocol) and execute
    /// any necessary functions.
    /// @param[in]  data    The received data/buffer to process.
    /// @param[in]  size    The size of the buffer to process.
    /// @return The number of bytes that were processed.
    uint16_t uartFrameProtocol_processRxData(uint8_t const data[], uint16_t size);
    
    /// Generates the formatted data to transmit out.  The formatted data to
    /// transmit will have the 0xaa frame characters and escape characters as
    /// necessary.
    /// @param[in]  source      The source buffer.
    /// @param[in]  sourceSize  The number of bytes in the source.
    /// @param[out] target      The target buffer (where the formatted data is
    ///                         stored).
    /// @param[in]  targetSize  The number of bytes available in the target.
    /// @return The number of bytes in the target buffer or the number of bytes
    ///         to transmit.  If 0, then the source buffer was either invalid or
    ///         there's not enough bytes in target buffer to store the formatted
    ///         data.
    uint16_t uartFrameProtocol_makeFormattedTxData(uint8_t const source[], uint16_t sourceSize, uint8_t target[], uint16_t targetSize);
    
    uint16_t uartFrameProtocol_processTx(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_FRAME_PROTOCOL_H


/* [] END OF FILE */
