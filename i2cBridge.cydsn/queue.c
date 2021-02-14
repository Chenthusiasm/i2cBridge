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

uint16_t getEnqueueDataOffset(Queue const volatile* queue)
{
    uint16_t offset = 0;
    if (!queue_isEmpty(queue))
    {
        uint16_t index = (queue->tail + queue->queueSize) % queue->queueSize;
        offset = queue->queue[index].dataOffset + queue->queue[index].dataSize;
    }
    return offset;
}


// === PUBLIC FUNCTIONS ========================================================

void queue_empty(Queue volatile* queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
}


bool queue_isFull(Queue const volatile* queue)
{
    return (queue->size == queue->queueSize);
}


bool queue_isEmpty(Queue const volatile* queue)
{
    return (queue->size == 0);
}


bool queue_enqueue(Queue volatile* queue, uint8_t* data, uint16_t length)
{
    bool status = false;
    if (!queue_isFull(queue))
    {
        uint16_t offset = getEnqueueDataOffset(queue);
        if ((offset + length) <= queue->dataSize)
        {
            QueueEntry* tail = &queue->queue[queue->tail];
            tail->dataOffset = offset;
            tail->dataSize = length;
            memcpy(tail, data, length);
            queue->size++;
            queue->tail++;
            if (queue->tail >= queue->queueSize)
                queue->tail = 0;
            status = true;
        }
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
        if (queue->head >= queue->queueSize)
            queue->head = 0;
    }
    return length;
}


uint16_t queue_peak(Queue const volatile* queue, uint8_t** data)
{
    uint16_t length = 0;
    *data = NULL;
    if (!queue_isEmpty(queue))
    {
        *data = &queue->pData[queue->queue[queue->head].dataOffset];
        length = queue->queue[queue->head].dataSize;
    }
    
    return length;
}


/* [] END OF FILE */
