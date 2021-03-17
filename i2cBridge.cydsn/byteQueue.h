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

#ifndef BYTE_QUEUE_H
    #define BYTE_QUEUE_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    /// Definition of the uint8_t queue.
    typedef struct ByteQueue_
    {
        /// Pointer to array that holds the bytes.
        uint8_t* data;
        
        /// The maximum number of bytes that can fit in data.
        uint16_t maxDataSize;
        
        /// The head of the queue, data is dequeued (removed) from the head.
        uint16_t head;
        
        /// The tail of the queue, data is enqueued (added) to the tail.
        uint16_t tail;
        
        /// The number of bytes currently in the queue.
        uint16_t size;
        
    } ByteQueue;
    
    
    // === FUNCTIONS ===========================================================
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // QUEUE_U8_H


/* [] END OF FILE */
