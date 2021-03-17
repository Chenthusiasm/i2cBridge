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
    
    /// Empty the queue. Note that the data array holding will not be cleared;
    /// residual data will remain. Because the empty operation modifies the
    /// queue data structure, DO NOT empty the queue in an ISR unless the queue
    /// is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    void byteQueue_empty(ByteQueue volatile* queue);
    
    /// Check if the queue is full; if the queue is full, subsequent enqueues
    /// will fail.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return If the queue is full.
    bool byteQueue_isFull(ByteQueue const volatile* queue);
    
    /// Check if the queue is empty; there are no queue elements in the queue.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return If the queue is empty.
    bool byteQueue_isEmpty(ByteQueue const volatile* queue);
    
    /// Enqueue (add) multiple new bytes into the queue tail (end). Because an
    /// enqueue modifies the queue data structure, DO NOT enqueue in an ISR
    /// unless the queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[in]  data    The data to enqueue.
    /// @param[in]  size    The size of the data (in bytes) to enqueue.
    /// @return If the enqueue operation was successful.
    bool bytequeue_enqueue(ByteQueue volatile* queue, uint8_t const* data, uint16_t size);
    
    /// Enqueue (add) a new byte into the queue tail (end). Because an enqueue
    /// modifies the queue data structure, DO NOT enqueue in an ISR unless the
    /// queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[in]  data    The data to enqueue.
    /// @return If the enqueue operation was successful.
    bool bytequeue_enqueueByte(ByteQueue volatile* queue, uint8_t data);
    
    /// Dequeue (remove) a variable number of bytes from the queue head (front).
    /// Because a dequeue modifies the queue data structure, DO NOT dequeue in
    /// an ISR unless the queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[out] data    Pointer to the dequeued data from the queue.
    /// @param[in]  size    The number of bytes to dequeue.
    /// @return The number of bytes that were dequeued.
    uint16_t byteQueue_dequeue(ByteQueue volatile* queue, uint8_t** data, uint16_t size);
    
    /// Dequeue (remove) a byte from the queue head (front). Because a dequeue
    /// modifies the queue data structure, DO NOT dequeue in an ISR unless the
    /// queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return The dequeued byte. If -1, then an error occured.
    int byteQueue_dequeueByte(ByteQueue volatile* queue);
    
    /// Get the data from the queue head (front). This operation is different
    /// from dequeue because the head will stay in the queue and the queue size
    /// will stay the same.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[out] data    Pointer to the peaked data from the queue.
    /// @param[in]  size    The number of bytes to peak.
    /// @return The size of the queue element that was dequeued.
    uint16_t byteQueue_peak(ByteQueue const volatile* queue, uint8_t** data, uint16_t size);
    
    /// Get the byte from the queue head (front). This operation is different
    /// from dequeue because the head will stay in the queue and the queue size
    /// will stay the same.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return The peaked byte. If -1, then an error occured.
    int byteQueue_peakByte(ByteQueue const volatile* queue);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // QUEUE_U8_H


/* [] END OF FILE */
