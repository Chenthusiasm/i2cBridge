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


// === DEFINES =================================================================

/// Size of the receive data buffer.
#define RX_BUFFER_SIZE                  (260u)

/// The max size of the transmit queue (the max number of queue elements).
#define TX_QUEUE_MAX_SIZE               (8u)

/// The size of the data array that holds the queue element data in the transmit
/// queue.
#define TX_QUEUE_DATA_SIZE              (600u)


// === CONSTANTS ===============================================================

/// Address of the slave I2C app.
static uint8_t const G_AppAddress = 0x48;

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

/// Flag indicating if the IRQ triggerd and a receive is pending consumption.
static volatile bool g_rxPending = false;

/// The receive buffer.
static uint8_t g_rxBuffer[RX_BUFFER_SIZE];

/// The receive callback function.
static I2CGen2_RxCallback g_rxCallback = NULL;

/// Array of transmit queue elements for the transmit queue.
static QueueElement g_txQueueElements[TX_QUEUE_MAX_SIZE];

/// Array to hold the data of the elements in the transmit queue.
static uint8_t g_txQueueData[TX_QUEUE_DATA_SIZE];

/// Transmit queue.
static Queue g_txQueue =
{
    g_txQueueData,
    g_txQueueElements,
    NULL,
    TX_QUEUE_DATA_SIZE,
    TX_QUEUE_MAX_SIZE,
    0,
    0,
    0,
    0,
};

/// The I2C address associated with the data that is waiting to be enqueued into
/// the transmit queue. This must be set prior to enqueueing data into the
/// transmit queue.
static uint8_t g_pendingTxEnqueueAddress = 0;

/// The status of the last driver API call. Refer to the possible error messages
/// in the generated "I2C Component Name"_I2C.h file.
static uint32_t g_lastDriverStatus = 0;


// === PRIVATE FUNCTIONS =======================================================

/// Resets the variables associated with the pending transmit enqueue.
void resetPendingTxEnqueue(void)
{
    g_pendingTxEnqueueAddress = 0;
}


/// Checks to see if the slave I2C bus is ready.
/// @return If the bus is ready for a new read/write transaction.
static bool isBusReady(void)
{
    return ((slaveI2C_I2CMasterStatus() & slaveI2C_I2C_MSTAT_XFER_INP) != 0);
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
static bool isIRQAsserted(void)
{
    return (slaveIRQPin_Read() == 0);
}


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
static void resetIRQ(void)
{
    static uint8_t clear[] = { AppBufferOffset_Response, 0 };
    slaveI2C_I2CMasterWriteBuf(G_AppAddress, clear, sizeof(clear), slaveI2C_I2C_MODE_COMPLETE_XFER);
}


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
        target[size++] = g_pendingTxEnqueueAddress;
        memcpy(&target[size], source, sourceSize);
        size += sourceSize;
    }
    return size;
}


// === ISR =====================================================================

/// ISR for the slaveIRQ (for the slaveIRQPin). The IRQ is asserted when there's
/// pending I2C data to be read from the I2C slave.
CY_ISR(slaveISR)
{
    slaveIRQ_ClearPending();
    slaveIRQPin_ClearInterrupt();
        
    g_rxPending = true;
}


// === PUBLIC FUNCTIONS ========================================================

void i2cGen2_init(void)
{
    // Configures the transmit variables.
    queue_registerEnqueueCallback(&g_txQueue, prepareTxQueueData);
    queue_empty(&g_txQueue);
    
    slaveI2C_Start();
    
    slaveIRQ_StartEx(slaveISR);
}


void i2cGen2_registerRxCallback(I2CGen2_RxCallback pCallback)
{
    if (pCallback != NULL)
        g_rxCallback = pCallback;
}


int i2cGen2_processRx(void)
{
    int length = 0;
    if (g_rxPending && isIRQAsserted())
    {
        if (isBusReady())
        {
            if (slaveI2C_I2CMasterReadBuf(G_AppAddress, g_rxBuffer, G_AppRxPacketLengthSize, slaveI2C_I2C_MODE_NO_STOP))
            {
                length += (int)G_AppRxPacketLengthSize;
                uint8_t dataLength = g_rxBuffer[AppRxPacketOffset_Length];
                if (isAppPacketLengthValid(dataLength))
                {
                    if (dataLength > 0)
                        slaveI2C_I2CMasterReadBuf(G_AppAddress, &g_rxBuffer[AppRxPacketOffset_Data], dataLength, slaveI2C_I2C_MODE_REPEAT_START);
                    else
                        slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeoutMS);
                    length += (int)dataLength;
                    if (g_rxCallback != NULL)
                        g_rxCallback(g_rxBuffer, (uint16_t)length);
                }
                else
                {
                    slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeoutMS);
                    length = -1;
                }
            }
            resetIRQ();
            g_rxPending = false;
        }
        else
            length = -1;
    }
    return length;
}


int i2cGen2_processTxQueue(uint32_t timeoutMS, bool quitIfBusy)
{
    Alarm alarm;
    if (timeoutMS > 0)
        alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
    else
        alarm_disarm(&alarm);
        
    int count = 0;
    while (!queue_isEmpty(&g_txQueue))
    {
        if (alarm_hasElapsed(&alarm))
        {
            count = -1;
            break;
        }
        if (isBusReady())
        {
            uint8_t* data;
            uint16_t size = queue_dequeue(&g_txQueue, &data);
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
    return count;
}


I2CGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size)
{
    I2CGen2Status status;
    status.errorOccurred = false;
    if ((data != NULL) && (size > 0))
    {
        if (isBusReady())
        {
            g_lastDriverStatus = slaveI2C_I2CMasterReadBuf(address, data, size, slaveI2C_I2C_MODE_COMPLETE_XFER);
            if (g_lastDriverStatus != slaveI2C_I2C_MSTR_NO_ERROR)
                status.driverError = true;
        }
        else
            status.busBusy = true;
    }
    else
        status.inputParametersInvalid = true;
    return status;
}


I2CGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    bool status = false;
    if ((data != NULL) && (size > 0) && isBusReady())
        status = (slaveI2C_I2CMasterWriteBuf(address, data, size, slaveI2C_I2C_MODE_COMPLETE_XFER) == slaveI2C_I2C_MSTR_NO_ERROR);
    return status;
}


I2CGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size)
{
    static uint8_t const MinSize = 2u;
    static uint8_t const AddressOffset = 0u;
    static uint8_t const DataOffset = 1u;
    
    bool status = false;
    if ((data != NULL) && (size > MinSize))
    {
        size--;
        status = i2cGen2_write(data[AddressOffset], &data[DataOffset], size);
    }
    return status;
}


I2CGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size)
{
    bool status = false;
    if (!queue_isFull(&g_txQueue))
    {
        g_pendingTxEnqueueAddress = address;
        status = queue_enqueue(&g_txQueue, data, size);
    }
    return status;
}


I2CGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size)
{
    bool status = false;
    if ((data != NULL) && (size > 0) && !queue_isFull(&g_txQueue))
    {
        g_pendingTxEnqueueAddress = data[TxQueueDataOffset_Address];
        status = queue_enqueue(&g_txQueue, &data[TxQueueDataOffset_Data], size - 1);
    }
    return status;
}


I2CGen2Status i2cGen2_appACK(uint32_t timeoutMS)
{
    Alarm alarm;
    if (timeoutMS > 0)
        alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
    else
        alarm_disarm(&alarm);
        
    bool status = false;
    while (!alarm_hasElapsed(&alarm))
    {
        // Scratch buffer; used so that the I2C read function has a valid non-
        // NULL pointer for reading 0 bytes.
        uint8_t scratch;
        if (isBusReady())
            status = (slaveI2C_I2CMasterReadBuf(G_AppAddress, &scratch, 0, slaveI2C_I2C_MODE_COMPLETE_XFER) == slaveI2C_I2C_MSTR_NO_ERROR);
    }
    return status;
}


/* [] END OF FILE */
