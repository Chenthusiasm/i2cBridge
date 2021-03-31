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

#include "i2cGen2.h"

#include <stdio.h>

#include "alarm.h"
#include "project.h"
#include "queue.h"
#include "utility.h"


// === DEFINES =================================================================

/// Name of the slave I2C component.
#define SLAVE_I2C                       slaveI2c_

/// Name of the slave IRQ component.
#define SLAVE_IRQ                       slaveIrq_

/// Name of the slave IRQ pin component.
#define SLAVE_IRQ_PIN                   slaveIrqPin_

/// Address of the default slave I2C app.
#define DEFAULT_SLAVE_ADDRESS           (0x48)

/// Size of the receive data buffer.
#define RX_BUFFER_SIZE                  (260u)

/// The max size of the transmit queue (the max number of queue elements).
#define TX_QUEUE_MAX_SIZE               (8u)

/// The size of the data array that holds the queue element data in the transmit
/// queue.
#define TX_QUEUE_DATA_SIZE              (600u)


// === TYPE DEFINES ============================================================

/// Definition of the transmit queue data offsets.
typedef enum TxQueueDataOffset_
{
    /// The I2C address.
    TxQueueDataOffset_Address           = 0u,
    
    /// The start of the data payload.
    TxQueueDataOffset_Data              = 1u,
    
} TxQueueDataOffset;


/// Definition of the duraTOUCH application I2C communication receive packet
/// offsets.
typedef enum AppRxPacketOffset_
{
    /// Command byte offset.
    AppRxPacketOffset_Command           = 0u,
    
    /// Length byte offset.
    AppRxPacketOffset_Length            = 1u,
    
    /// Data payload offset.
    AppRxPacketOffset_Data              = 2u,
    
} AppRxPacketOffset;


/// Definition of the duraTOUCH application I2C communication transmit packet
/// offsets.
typedef enum AppTxPacketOffset_
{
    /// Length byte offset.
    AppTxPacketOffset_BufferOffset      = 0u,
    
    /// Data payload offset.
    AppTxPacketOffset_Data              = 1u,
    
} AppTxPacketOffset;


/// Definition of the duraTOUCH application memory buffer offset used for
/// setting the app buffer offset in tramsit packets.
typedef enum AppBufferOffset_
{
    /// Command buffer offset; used to write a command.
    AppBufferOffset_Command             = 0x00,
    
    /// Response buffer offset; used to read a command.
    AppBufferOffset_Response            = 0x20,
    
} AppBufferOffset;


/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap_
{
    /// Transmit queue.
    Queue txQueue;
    
    /// Array of transmit queue elements for the transmit queue.
    QueueElement txQueueElements[TX_QUEUE_MAX_SIZE];
    
    /// Array to hold the data of the elements in the transmit queue.
    uint8_t txQueueData[TX_QUEUE_DATA_SIZE];
    
    /// The receive buffer.
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    
    /// The I2C address associated with the data that is waiting to be enqueued
    /// into the transmit queue. This must be set prior to enqueueing data into
    /// the transmit queue.
    uint8_t pendingTxEnqueueAddress;
    
} Heap;


// === CONSTANTS ===============================================================

/// The number of bytes to read in order to find the number of bytes in the
/// receive data payload.
static uint8_t const G_AppRxPacketLengthSize = AppRxPacketOffset_Length + 1;

/// The value of the length byte which indicates the packet is invalid.
static uint8_t const G_InvalidRxAppPacketLength = 0xff;

/// The default amount of time to allow for a I2C stop condition to be sent
/// before timing out. If a time out occurs, the I2C module is reset. This is
/// in milliseconds.
static uint32_t const G_DefaultSendStopTimeoutMS = 5u;


// === GLOBALS =================================================================

/// @TODO: remove this when ready to use the dynamic memory allocation.
static Heap g_tempHeap;

/// Heap-like memory that points to the global variables used by the module that
/// was dynamically allocated. If NULL, then the module's global variables
/// have not been dynamically allocated and the module has not started.
static Heap* g_heap = NULL;

/// The current 7-bit slave address. When the slaveIRQ line is asserted, a read
/// will be performed from this address.
static uint8_t g_slaveAddress = DEFAULT_SLAVE_ADDRESS;

/// Flag indicating if the IRQ triggerd and a receive is pending consumption.
static volatile bool g_rxPending = false;

/// The receive callback function.
static I2CGen2_RxCallback g_rxCallback = NULL;

/// The status of the last driver API call. Refer to the possible error messages
/// in the generated "I2C Component Name"_I2C.h file.
static uint32_t g_lastDriverStatus = 0;


// === PRIVATE FUNCTIONS =======================================================

/// Generates the transmit queue data to include the I2C address as the first
/// byte (encode the I2C address in the data). The transmit dequeue function
/// will take care of properly pulling out the I2C address and the actual data
/// payload to transmit. Note that the g_pendingTxEnqueueAddress must be set
/// properly before invoking this function.
/// @param[in]  source      The source buffer.
/// @param[in]  sourceSize  The number of bytes in the source.
/// @param[out] target      The target buffer (where the formatted data is
///                         stored).
/// @param[in]  targetSize  The number of bytes available in the target.
/// @return The number of bytes in the target buffer or the number of bytes
///         to transmit.  If 0, then the source buffer was either invalid or
///         there's not enough bytes in target buffer to store the formatted
///         data.
static uint16_t prepareTxQueueData(uint8_t target[], uint16_t targetSize, uint8_t const source[], uint16_t sourceSize)
{
    static uint16_t const MinSourceSize = TxQueueDataOffset_Data + 1;
    
    uint16_t size = 0;
    if ((source != NULL) && (sourceSize >= MinSourceSize) && (target != NULL) && (targetSize > sourceSize))
    {
        target[size++] = g_heap->pendingTxEnqueueAddress;
        memcpy(&target[size], source, sourceSize);
        size += sourceSize;
    }
    return size;
}

/// Initializes the transmit queue.
static void initTxQueue(void)
{
    queue_registerEnqueueCallback(&g_heap->txQueue, prepareTxQueueData);
    g_heap->txQueue.data = g_heap->txQueueData;
    g_heap->txQueue.elements = g_heap->txQueueElements;
    g_heap->txQueue.maxDataSize = TX_QUEUE_DATA_SIZE;
    g_heap->txQueue.maxSize = TX_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->txQueue);
}


/// Resets the variables associated with the pending transmit enqueue.
void resetPendingTxEnqueue(void)
{
    g_heap->pendingTxEnqueueAddress = 0;
}


/// Checks to see if the slave I2C bus is ready.
/// @return If the bus is ready for a new read/write transaction.
static bool isBusReady(void)
{
    return ((COMPONENT(SLAVE_I2C, I2CMasterStatus)() & COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_INP)) != 0);
}


/// Checks if the read packet contains a valid data payload length.
/// @param[in]  length  The data payload length.
/// @return If the length is valid for a read packet.
static bool isAppPacketLengthValid(uint8_t length)
{
    return (length < G_InvalidRxAppPacketLength);
}


/// Checks to see if the slave IRQ pin has been asserted, meaning there's data
/// ready to be read from the slave device.
/// @return If the slave IRQ pin is asserted.
static bool isIrqAsserted(void)
{
    return (COMPONENT(SLAVE_IRQ_PIN, Read)() == 0);
}


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
static void resetIrq(void)
{
    static uint8_t clear[] = { AppBufferOffset_Response, 0 };
    COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(g_slaveAddress, clear, sizeof(clear), COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
}


// === ISR =====================================================================

/// ISR for the slaveIRQ (for the slaveIRQPin). The IRQ is asserted when there's
/// pending I2C data to be read from the I2C slave.
CY_ISR(slaveIsr)
{
    COMPONENT(SLAVE_IRQ, ClearPending)();
    COMPONENT(SLAVE_IRQ_PIN, ClearInterrupt)();
        
    g_rxPending = true;
}


// === PUBLIC FUNCTIONS ========================================================

void i2cGen2_init(void)
{
    i2cGen2_resetSlaveAddress();
    
    COMPONENT(SLAVE_I2C, Start)();
    COMPONENT(SLAVE_IRQ, StartEx)(slaveIsr);
}


uint16_t i2cGen2_getMemoryRequirement(void)
{
    uint16_t const Mask = sizeof(uint32_t) - 1;
    
    uint16_t size = sizeof(Heap);
    if ((size & Mask) != 0)
        size = (size + sizeof(uint32_t)) & ~Mask;
    return size;
}


uint16_t i2cGen2_activate(uint32_t memory[], uint16_t size)
{
    uint32_t allocatedSize = 0;
    if ((memory != NULL) && (sizeof(Heap) <= (sizeof(uint32_t) * size)))
    {
        g_heap = (Heap*)memory;
        // @TODO: remove the following line when the dynamic memory allocation
        // is ready.
        g_heap = &g_tempHeap;
        initTxQueue();
        allocatedSize = i2cGen2_getMemoryRequirement() / sizeof(uint32_t);
    }
    return allocatedSize;
}
 

void i2cGen2_deactivate(void)
{
    g_heap = NULL;
}


void i2cGen2_setSlaveAddress(uint8_t address)
{
    g_slaveAddress = address;
}


void i2cGen2_resetSlaveAddress(void)
{
    g_slaveAddress = DEFAULT_SLAVE_ADDRESS;
}

    
void i2cGen2_registerRxCallback(I2CGen2_RxCallback pCallback)
{
    if (pCallback != NULL)
        g_rxCallback = pCallback;
}


int i2cGen2_processRx(void)
{
    int length = 0;
    if (g_heap != NULL)
    {
        if (g_rxPending && isIrqAsserted())
        {
            if (isBusReady())
            {
                if (COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(g_slaveAddress, g_heap->rxBuffer, G_AppRxPacketLengthSize, COMPONENT(SLAVE_I2C, I2C_MODE_NO_STOP)))
                {
                    length += (int)G_AppRxPacketLengthSize;
                    uint8_t dataLength = g_heap->rxBuffer[AppRxPacketOffset_Length];
                    if (isAppPacketLengthValid(dataLength))
                    {
                        if (dataLength > 0)
                            COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(g_slaveAddress, &g_heap->rxBuffer[AppRxPacketOffset_Data], dataLength, COMPONENT(SLAVE_I2C, I2C_MODE_REPEAT_START));
                        else
                            COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
                        length += (int)dataLength;
                        if (g_rxCallback != NULL)
                            g_rxCallback(g_heap->rxBuffer, (uint16_t)length);
                    }
                    else
                    {
                        COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
                        length = -1;
                    }
                }
                resetIrq();
                g_rxPending = false;
            }
            else
                length = -1;
        }
    }
    else
        length = -1;
    return length;
}


int i2cGen2_processTxQueue(uint32_t timeoutMS, bool quitIfBusy)
{
    int count = 0;
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
        else
            alarm_disarm(&alarm);
            
        int count = 0;
        while (!queue_isEmpty(&g_heap->txQueue))
        {
            if (alarm_hasElapsed(&alarm))
            {
                count = -1;
                break;
            }
            if (isBusReady())
            {
                uint8_t* data;
                uint16_t size = queue_dequeue(&g_heap->txQueue, &data);
                if (size > 0)
                {
                    i2cGen2_write(data[TxQueueDataOffset_Address], &data[TxQueueDataOffset_Data], size - 1);
                    ++count;
                }
            }
            else if (quitIfBusy)
            {
                count = -1;
                break;
            }
        }
    }
    else
        count = -1;
    return count;
}


I2CGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                    status.driverError = true;
                g_lastDriverStatus = driverStatus;
            }
            else
                status.busBusy = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2CGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, data, size, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                    status.driverError = true;
                g_lastDriverStatus = driverStatus;
            }
            else
                status.busBusy = true;

        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2CGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size)
{
    static uint8_t const MinSize = 2u;
    static uint8_t const AddressOffset = 0u;
    static uint8_t const DataOffset = 1u;
    
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > MinSize))
        {
            size--;
            status = i2cGen2_write(data[AddressOffset], &data[DataOffset], size);
        }
    }
    else
        status.deactivated = true;
    return status;
}


I2CGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (!queue_isFull(&g_heap->txQueue))
            {
                g_heap->pendingTxEnqueueAddress = address;
                if (!queue_enqueue(&g_heap->txQueue, data, size))
                    status.transmitQueueFull = true;
            }
            else
                status.transmitQueueFull = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2CGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if(!queue_isFull(&g_heap->txQueue))
            {
                g_heap->pendingTxEnqueueAddress = data[TxQueueDataOffset_Address];
                if (!queue_enqueue(&g_heap->txQueue, &data[TxQueueDataOffset_Data], size - 1))
                    status.transmitQueueFull = true;
            }
            else
                status.inputParametersInvalid = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2CGen2Status i2cGen2_appACK(uint32_t timeoutMS)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
        else
            alarm_disarm(&alarm);
            
        
        while (true)
        {
            if (alarm_hasElapsed(&alarm))
            {
                status.busBusy = true;
                break;
            }
            
            // Scratch buffer; used so that the I2C read function has a valid non-
            // NULL pointer for reading 0 bytes.
            uint8_t scratch;
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(g_slaveAddress, &scratch, 0, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                {
                    status.driverError = true;
                    if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
                        status.nak = true;
                }
                g_lastDriverStatus = driverStatus;
            }
        }
    }
    else
        status.deactivated = true;
    return status;
}


/* [] END OF FILE */
