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

#ifndef UART_TRANSLATE_H
    #define UART_TRANSLATE_H

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
    
    
    // === DEFINES =============================================================
    
    /// The max size of the receive queue (the max number of queue elements).
    #define TRANSLATE_RX_QUEUE_MAX_SIZE                     (8u)
    
    /// The size of the data array that holds the queue element data in the
    /// receive queue.
    #define TRANSLATE_RX_QUEUE_DATA_SIZE                    (600u)
    
    /// The max size of the transmit queue (the max number of queue elements).
    #define TRANSLATE_TX_QUEUE_MAX_SIZE                     (8u)
    
    /// The size of the data array that holds the queue element data in the
    /// transmit queue.
    #define TRANSLATE_TX_QUEUE_DATA_SIZE                    (800u)
    
    
    // === TYPED DEFINES =======================================================
    
    /// Data extension for the Heap structure. Defines the data buffers when in
    /// the normal translate mode.
    typedef struct TranslateHeapData
    {
        /// Array of decoded receive queue elements for the receive queue; these
        /// elements have been received but are pending processing.
        QueueElement decodedRxQueueElements[TRANSLATE_RX_QUEUE_MAX_SIZE];
        
        /// Array of transmit queue elements for the transmit queue.
        QueueElement txQueueElements[TRANSLATE_TX_QUEUE_MAX_SIZE];
        
        /// Array to hold the decoded data of elements in the receive queue.
        uint8_t decodedRxQueueData[TRANSLATE_RX_QUEUE_DATA_SIZE];
        
        /// Array to hold the data of the elements in the transmit queue.
        uint8_t txQueueData[TRANSLATE_TX_QUEUE_DATA_SIZE];
        
    } TranslateHeapData;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the communications interface.
    void uartTranslate_init(void);
    
    /// Accessor to get the number of heap words required for global variables.
    /// @return The number of heap words needed for global variables.
    uint16_t uartTranslate_getHeapWordRequirement(void);
    
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
    uint16_t uartTranslate_activate(heapWord_t memory[], uint16_t size);
    
    /// Deactivates the UART frame protocol module and effectively deallocates
    /// the global memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t uartTranslate_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// normal mode.
    /// @return If normal mode is activated.
    bool uartTranslate_isActivated(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_TRANSLATE_H


/* [] END OF FILE */
