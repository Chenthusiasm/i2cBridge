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

#include "byteQueue.h"

#include <stdio.h>
#include <string.h>


// === PRIVATE FUNCTIONS =======================================================

/// Gets the number of bytes free or open in the queue to enqueue additional
/// bytes.
/// @param[in]  queue   The queue to check. The function assumes this is non-
///                     null.
/// @return The number of free bytes.
static uint16_t getFreeSize(ByteQueue const volatile* queue)
{
    return (queue->maxSize - queue->size);
}


/// Gets the number of bytes free or open in the queue to the end of the data
/// buffer. Data cannot be added beyond this point so overflow bytes have to be
/// added back to the beginning of the data buffer.
/// @param[in]  queue   The queue to check. The function assumes this is non-
///                     null.
/// @return The number of free bytes until the end of the data buffer.
static uint16_t getFreeSizeToEnd(ByteQueue const volatile* queue)
{
    return (queue->maxSize - queue->tail);
}


// === PUBLIC FUNCTIONS ========================================================

void byteQueue_empty(ByteQueue volatile* queue)
{
    if (queue != NULL)
    {
        queue->head = 0;
        queue->tail = 0;
        queue->size = 0;
    }
}


bool byteQueue_isFull(ByteQueue const volatile* queue)
{
    bool status = true;
    if (queue != NULL)
        status = (queue->size >= queue->maxSize);
    return status;
}


bool byteQueue_isEmpty(ByteQueue const volatile* queue)
{
    bool status = true;
    if (queue != NULL)
        status = (queue->size <= 0);
    return status;
}


bool bytequeue_enqueue(ByteQueue volatile* queue, uint8_t const data[], uint16_t size)
{
    bool status = false;
    if (!byteQueue_isFull(queue) && (data != NULL) && (size > 0) && (size <= getFreeSize(queue)))
    {
        uint16_t copySize = getFreeSizeToEnd(queue);
        if (copySize > size)
            copySize = size;
        memcpy(&queue->data[queue->tail], data, copySize);
        if (copySize < size)
        {
            data += copySize;
            copySize = size - copySize;
            memcpy(queue->data, data, copySize);
            queue->tail = copySize;
        }
        else
        {
            queue->tail += copySize;
            if (queue->tail >= queue->maxSize)
                queue->tail = 0;
        }
        queue->size += size;
        status = true;
    }
    return status;
}


bool bytequeue_enqueueByte(ByteQueue volatile* queue, uint8_t data)
{
    bool status = false;
    if (!byteQueue_isFull(queue))
    {
        queue->data[queue->tail++] = data;
        if (queue->tail >= queue->maxSize)
            queue->tail = 0;
        --queue->size;
        status = true;
    }
    return status;
}


uint16_t byteQueue_dequeue(ByteQueue volatile* queue, uint8_t data[], uint16_t size)
{
    if (!byteQueue_isEmpty(queue) && (data != NULL) && (size > 0))
    {
        if (size > queue->size)
            size = queue->size;
        uint16_t copySize = size;
        if ((queue->head + copySize) > queue->maxSize)
        {
            copySize = queue->maxSize - queue->head;
        }    
        memcpy(data, &queue->data[queue->head], copySize);
        if (copySize < size)
        {
            data += copySize;
            copySize = size - copySize;
            memcpy(data, queue->data, copySize);
            queue->head = copySize;
        }
        else
        {
            queue->head += copySize;
            if (queue->head >= queue->maxSize)
                queue->head = 0;
        }
        queue->size -= size;
        
        // If the queue is empty, empty the queue to reset the head and tail.
        if (byteQueue_isEmpty(queue))
            byteQueue_empty(queue);
    }
    else
        size = 0;
    return size;
}


int byteQueue_dequeueByte(ByteQueue volatile* queue)
{
    int data = -1;
    if (!byteQueue_isEmpty(queue))
    {
        data = (int)queue->data[queue->head++];
        if (queue->head >= queue->maxSize)
            queue->head;
        --queue->size;
        
        // If the queue is empty, empty the queue to reset the head and tail.
        if (byteQueue_isEmpty(queue))
            byteQueue_empty(queue);
    }
    return data;
}


uint16_t byteQueue_peak(ByteQueue const volatile* queue, uint8_t data[], uint16_t size);

int byteQueue_peakByte(ByteQueue const volatile* queue);


/* [] END OF FILE */
