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

#ifndef UART_UPDATE_H
    #define UART_UPDATE_H

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
    
    /// Union that holds the status of update functions.
    typedef struct UpdateStatus
    {
        /// 8-bit representation of the status. Used to get the bit mask created
        /// by the following anonymous struct of 1-bit flags.
        uint8_t mask;
        
        /// Anonymous struct of 1-bit flags indicating specific errors.
        struct
        {
            /// Error tabulating the checksum of the flash row that was to be
            /// flashed.
            bool flashRowChecksumError : 1;
            
            /// Attempted to update a flash row that is protected (cannot be
            /// reflashed).
            bool flashProtectionError : 1;
            
            /// Failed to verify the packet checksum.
            bool packetChecksumError : 1;
            
            /// Invalid key was sent.
            bool invalidKey : 1;
            
            /// Invalid command.
            bool invalidCommand : 1;
            
            /// A split-packet error occurred.
            bool splitPacketError : 1;
        };
        
    } UpdateStatus;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Accessor to get the number of heap words required for global variables.
    /// @return The number of heap words needed for global variables.
    uint16_t uartUpdate_getHeapWordRequirement(void);
    
    /// Activates the UART frame protocol module and sets up its globals.
    /// This must be invoked before using any processRx or processTx-like
    /// functions.
    /// @param[in]  memory          Memory buffer that is available for the
    ///                             module's globals. This is an array of 32-bit
    ///                             worlds to help preserve word alignment.
    /// @param[in]  size            The size (in 32-bit words) of the memory
    ///                             array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t uartUpdate_activate(heapWord_t memory[], uint16_t size);
    
    /// Deactivates the UART frame protocol module and effectively deallocates
    /// the global memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t uartUpdate_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// normal mode.
    /// @return If normal mode is activated.
    bool uartUpdate_isActivated(void);
    
    /// General process function of any pending UART receive or transmit
    /// functionality when in update mode.
    /// @return If the pending UART transactions were executed successfully.
    bool uartUpdate_process(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_UPDATE_H


/* [] END OF FILE */
