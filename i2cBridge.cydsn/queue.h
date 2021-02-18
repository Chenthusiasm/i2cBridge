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
    
    /// Definition of the callback function that is invoked when data needs to
    /// be enqueued.
    typedef uint16_t (*Queue_EnqueueCallback)(uint8_t[], uint16_t, uint8_t const[], uint16_t);
    
    
    /// Definition of the individual element in the queue. This helps define
    /// where in the data buffer the element starts and how large it is.
    typedef struct QueueElement_
    {
        /// The start offset of the element from the queue's data array.
        uint16_t dataOffset;
        
        /// The number of bytes of data in the element.
        uint16_t dataSize;
        
    } QueueElement;
    
    
    /// Definition of the queue object.
    typedef struct Queue_
    {
        /// Data array that holds the raw data of each member of the queue.
        uint8_t* data;
        
        /// Array of queue elements.
        QueueElement* elements;
        
        /// Enqueue callback function.
        Queue_EnqueueCallback enqueueCallback;
        
        /// The maximum size (in bytes) in the data array.
        uint16_t maxDataSize;
        
        /// The maximum number of elements in queue.
        uint8_t maxSize;
        
        /// The head of the queue, entries are dequeue (removed) from the head.
        uint8_t head;
        
        /// The tail of the queue, entries are enqueued (added) to the tail.
        uint8_t tail;
        
        /// The number of elements currently in the queue.
        uint8_t size;
        
    } __attribute__((packed)) Queue;
    
    
    // === PUBLIC FUNCTIONS ====================================================
    
    void queue_empty(Queue volatile* queue);
    
    void queue_registerEnqueueCallback(Queue volatile* queue, Queue_EnqueueCallback callback);
    
    void queue_deregisterEnqueueCallback(Queue volatile* queue);
    
    bool queue_isFull(Queue const volatile* queue);
    
    bool queue_isEmpty(Queue const volatile* queue);
    
    bool queue_enqueue(Queue volatile* queue, uint8_t const* data, uint16_t size);
    
    uint16_t queue_dequeue(Queue volatile* queue, uint8_t** data);
    
    uint16_t queue_peak(Queue const volatile* queue, uint8_t** data);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // QUEUE_H


/* [] END OF FILE */
