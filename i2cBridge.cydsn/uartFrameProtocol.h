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
    typedef bool (*UartOutOfFrameRxCallback)(char);
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the communications interface.
    void uartFrameProtocol_init(void);
    
    /// Registers the receive callback function that should be invoked when
    /// data is received out of frame.
    /// @param[in]  pCallback   Pointer to the callback function.
    void uartFrameProtocol_registerOutOfFrameRxCallback(UartOutOfFrameRxCallback pCallback);
    
    /// Process the received data (parse using the UART protocol) and execute
    /// any necessary functions.
    /// @param[in]  pBuffer     The received data/buffer to process.
    /// @param[in]  bufferSize  The size of the buffer to process.
    /// @return The number of bytes that were processed.
    uint16_t uartFrameProtocol_processRxData(uint8_t const* pBuffer, uint16_t bufferSize);
    
    /// Generates the formatted data to transmit out.  The formatted data to
    /// transmit will have the 0xaa frame characters and escape characters as
    /// necessary.
    /// @param[in]  pSource         The source buffer.
    /// @param[in]  sourceLength    The number of bytes in the source.
    /// @param[out] pTarget         The target buffer (where the formatted data
    ///                             is stored).
    /// @param[in]  targetLength    The number of bytes available in the target.
    /// @return The number of bytes in the target buffer or the number of bytes
    ///         to transmit.  If 0, then the source buffer was either invalid or
    ///         there's not enough bytes in target buffer to store the formatted
    ///         data.
    uint16_t uartFrameProtocol_makeFormattedTxData(uint8_t const* pSource, uint16_t sourceLength, uint8_t* pTarget, uint16_t targetLength);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_FRAME_PROTOCOL_H


/* [] END OF FILE */
