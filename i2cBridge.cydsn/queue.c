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

// === DEPENDENCIES ============================================================

#include "queue.h"

#include <stdio.h>
#include <string.h>


// === PRIVATE FUNCTIONS =======================================================

/// Get the data offset in the data buffer that defines where the pending
/// enqueue data starts.
/// @param[in]  queue   The queue.
/// @return The data offset in the data buffer of the queue.
uint16_t getEnqueueDataOffset(Queue const volatile* queue)
{
    uint16_t offset = 0;
    if (!queue_isEmpty(queue))
    {
        uint16_t index = (queue->tail + queue->maxSize) % queue->maxSize;
        offset = queue->elements[index].dataOffset + queue->elements[index].dataSize;
    }
    return offset;
}


// === PUBLIC FUNCTIONS ========================================================

void queue_empty(Queue volatile* queue)
{
    if (queue != NULL)
    {
        queue->head = 0;
        queue->tail = 0;
        queue->size = 0;
    }
}


void queue_registerEnqueueCallback(Queue volatile* queue, Queue_EnqueueCallback callback)
{
    if ((queue != NULL) && (callback != NULL))
        queue->enqueueCallback = callback;
}
    

void queue_deregisterEnqueueCallback(Queue volatile* queue)
{
    if (queue != NULL)
        queue->enqueueCallback = NULL;
}

    
bool queue_isFull(Queue const volatile* queue)
{
    bool status = false;
    if (queue != NULL)
        status = (queue->size == queue->maxSize);
    return status;
}


bool queue_isEmpty(Queue const volatile* queue)
{
    bool status = false;
    if (queue != NULL)
        status = (queue->size == 0);
    return status;
}


bool queue_enqueue(Queue volatile* queue, uint8_t const* data, uint16_t size)
{
    bool status = false;
    if ((queue != NULL) && (data != NULL) && (size > 0) && !queue_isFull(queue))
    {
        uint16_t offset = getEnqueueDataOffset(queue);
        if ((offset + size) <= queue->maxDataSize)
        {
            uint16_t enqueueSize = size;
            if (queue->enqueueCallback != NULL)
                enqueueSize = queue->enqueueCallback(&queue->data[offset], queue->maxDataSize - offset, data, size);
            else
                memcpy(&queue->data[offset], data, size);
            
            // The enqueue is successful if enqueueSize > 0; if this is the
            // case, update the queue to incdicate a successful enqueue.
            if (enqueueSize > 0)
            {
                QueueElement* tail = &queue->elements[queue->tail];
                tail->dataOffset = offset;
                tail->dataSize = enqueueSize;
                queue->size++;
                queue->tail++;
                if (queue->tail >= queue->maxSize)
                    queue->tail = 0;
                status = true;
            }
            
            // No matter if the enqueue succeeds or fails, always reset the
            // pending enqueue size to 0 to indicate that the previous
            // byte-by-byte data has been stomped on.
            queue->pendingEnqueueSize = 0;
        }
    }
    return status;
}


bool queue_enqueueByte(Queue volatile* queue, uint8_t data, bool lastByte)
{
    bool status = false;
    if ((queue != NULL) && !queue_isFull(queue))
    {
        uint16_t enqueueSize = 1;
        uint16_t dataOffset = getEnqueueDataOffset(queue) + queue->pendingEnqueueSize;
        if (queue->enqueueCallback != NULL)
            enqueueSize = queue->enqueueCallback(&queue->data[dataOffset], queue->maxDataSize - dataOffset, &data, enqueueSize);
        else
            queue->data[dataOffset] = data;
            
        // The enqueue is successful if enqueueSize > 0; if this is the case,
        // update the queue to indicate a successful enqueue.
        if (enqueueSize > 0)
        {
            queue->pendingEnqueueSize += enqueueSize;
            if (lastByte)
                status = queue_enqueueFinalize(queue);
        }
    }
    return status;
}


bool queue_enqueueFinalize(Queue volatile* queue)
{
    bool status = false;
    if ((queue != NULL) && !queue_isFull(queue) && (queue->pendingEnqueueSize > 0))
    {
        QueueElement* tail = &queue->elements[queue->tail];
        tail->dataOffset = getEnqueueDataOffset(queue);
        tail->dataSize = queue->pendingEnqueueSize;
        queue->size++;
        queue->tail++;
        if (queue->tail >= queue->maxSize)
            queue->tail = 0;
        queue->pendingEnqueueSize = 0;
    }
    return status;
}


uint16_t queue_dequeue(Queue volatile* queue, uint8_t** data)
{
    uint16_t length = queue_peak(queue, data);
    if (length > 0)
    {
        queue->size--;
        queue->head++;
        if (queue->head >= queue->maxSize)
            queue->head = 0;
    }
    return length;
}


uint16_t queue_peak(Queue const volatile* queue, uint8_t** data)
{
    uint16_t length = 0;
    if ((queue != NULL) && (data != NULL) && !queue_isEmpty(queue))
    {
        *data = NULL;
        *data = &queue->data[queue->elements[queue->head].dataOffset];
        length = queue->elements[queue->head].dataSize;
    }
    return length;
}


/* [] END OF FILE */
