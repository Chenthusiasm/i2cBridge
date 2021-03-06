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
    typedef struct QueueElement
    {
        /// The start offset of the element from the queue's data array.
        uint16_t dataOffset;
        
        /// The number of bytes of data in the element.
        uint16_t dataSize;
        
    } QueueElement;
    
    
    /// Definition of the queue object. Size (32-bit) = 20.
    typedef struct Queue
    {
        /// Data array that holds the raw data of each member of the queue.
        uint8_t* data;
        
        /// Array of queue elements.
        QueueElement* elements;
        
        /// Enqueue callback function.
        Queue_EnqueueCallback enqueueCallback;
        
        /// The maximum size (in bytes) in the data array. If an enqueue occurs
        /// but the data cannot fit within the data array because the total
        /// number of bytes exceeds this value, the enqueue will fail.
        uint16_t maxDataSize;
        
        /// The size of the pending enqueue element. This is mainly used when
        /// the byte-by-byte enqueue is used (queue_enqueueByte). Otherwise,
        /// this value will remain 0.
        uint16_t pendingEnqueueSize;
        
        /// The maximum number of elements that can be queued.
        uint8_t maxSize;
        
        /// The head of the queue, entries are dequeued (removed) from the head.
        uint8_t head;
        
        /// The tail of the queue, entries are enqueued (added) to the tail.
        uint8_t tail;
        
        /// The number of elements currently in the queue.
        uint8_t size;
        
    } Queue;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Empty the queue; no queue elements will be in the queue. Note that the
    /// data array holding each queue element's data will not be cleared.
    /// Because the empty operation modifies the queue data structure, DO NOT
    /// empty the queue in an ISR unless the queue is protected by a mutex,
    /// semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    void queue_empty(Queue volatile* queue);
    
    /// Register a specific callback function to be invoked during an enqueue.
    /// If no callback function is registered, the default action is taken which
    /// is a byte-for-byte memory copy of the data to be enqueued.
    /// @param[in]  queue       The queue to perform the function's action on.
    /// @param[in]  callback    The callback function to register.
    void queue_registerEnqueueCallback(Queue volatile* queue, Queue_EnqueueCallback callback);
    
    /// Remove the currently registered callback function that was invoked when
    /// an enqueue occurs. The default action of a byte-for-byte memory copy
    /// will be performed when an enqueue occurs.
    /// @param[in]  queue       The queue to perform the function's action on.
    /// @param[in]  callback    The callback function to register.
    void queue_deregisterEnqueueCallback(Queue volatile* queue);
    
    /// Check if the queue is full; if the queue is full, subsequent enqueues
    /// will fail.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return If the queue is full.
    bool queue_isFull(Queue const volatile* queue);
    
    /// Check if the queue is empty; there are no queue elements in the queue.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @return If the queue is empty.
    bool queue_isEmpty(Queue const volatile* queue);
    
    /// Enqueue (add) a new queue element into the queue tail (end). Because an
    /// enqueue modifies the queue data structure, DO NOT enqueue in an ISR
    /// unless the queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[in]  data    The data to enqueue.
    /// @param[in]  size    The size of the data (in bytes) to enqueue.
    /// @return If the enqueue operation was successful.
    bool queue_enqueue(Queue volatile* queue, uint8_t const* data, uint16_t size);
    
    /// Enqueue (add) a new queue element into the queue tail (end) in a
    /// byte-by-byte fashion. Because enqueue modifies the queue data structure,
    /// DO NOT enqueue in an ISR unless the queue is protected by a mutex,
    /// semaphore, or lock.
    /// @param[in]  queue       The queue to perform the function's action on.
    /// @param[in]  data        The data to enqueue.
    /// @param[in]  lastByte    Flag indicating if the byte being enqueued is
    ///                         the last byte in the element (so the a full
    ///                         element should be queuened).
    /// @return If the enqueue operation was successful.
    bool queue_enqueueByte(Queue volatile* queue, uint8_t data, bool finalize);
    
    /// Enqueue (add) a new queue lement into the queue tail (end) based on
    /// the pending data added byte-by-byte by the queue_enqueueByte function.
    /// Because enqueue modifies the queue data structure, DO NOT enqueue in an
    /// ISR unless the queue is protected by a mutet, semaphore, or lock.
    bool queue_enqueueFinalize(Queue volatile* queue);
    
    /// Dequeue (remove) the oldest queue element from the queue head (front).
    /// Also provides access to the data from this queue element.  Because a
    /// dequeue modifies the queue data structure, DO NOT dequeue in an ISR
    /// unless the queue is protected by a mutex, semaphore, or lock.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[out] data    Pointer to the dequeued data from the queue.
    /// @return The size of the queue element that was dequeued.
    uint16_t queue_dequeue(Queue volatile* queue, uint8_t** data);
    
    /// Get the data from the oldest queue element from the queue head (front).
    /// This operation is different from dequeue because the head queue element
    /// will stay in the queue and the queue size will stay the same.
    /// @param[in]  queue   The queue to perform the function's action on.
    /// @param[out] data    Pointer to the dequeued data from the queue.
    /// @return The size of the queue element that was dequeued.
    uint16_t queue_peak(Queue const volatile* queue, uint8_t** data);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // QUEUE_H


/* [] END OF FILE */
