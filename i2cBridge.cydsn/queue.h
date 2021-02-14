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

#ifndef QUEUE_H
    #define QUEUE_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    ///
    typedef struct QueueEntry_
    {
        /// The start offset of the entry from the queue's data array.
        uint16_t dataOffset;
        
        /// The number of bytes of data in the node.
        uint16_t dataSize;
    } QueueEntry;
    
    
    typedef struct Queue_
    {
        /// Data array that holds the raw data of each member of the queue.
        uint8_t* pData;
        
        /// The size of the data array.
        uint16_t dataSize;
        
        /// Array of entries in the queue.
        QueueEntry* queue;
        
        /// The size of the array of entries (the max number of entries
        /// possible).
        uint8_t queueSize;
        
        /// The head of the queue, entries are dequeue (removed) from the head.
        uint8_t head;
        
        /// The tail of the queue, entries are enqueued (added) to the tail.
        uint8_t tail;
        
        /// The number
        uint8_t size;
    } __attribute__((packed)) Queue;
    
    
    // === PUBLIC FUNCTIONS ====================================================
    
    void queue_empty(Queue volatile* queue);
    
    bool queue_isFull(Queue const volatile* queue);
    
    bool queue_isEmpty(Queue const volatile* queue);
    
    bool queue_enqueue(Queue volatile* queue, uint8_t* pData, uint16_t length);
    
    uint16_t queue_dequeue(Queue volatile* queue, uint8_t** ppData);
    
    uint16_t queue_peak(Queue const volatile* queue, uint8_t** ppData);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // QUEUE_H


/* [] END OF FILE */
