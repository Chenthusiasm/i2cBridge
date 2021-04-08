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
#include "debug.h"
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

/// Size of the receive data buffer.
#define RX_BUFFER_SIZE                  (260u)

/// The max size of the transmit queue (the max number of queue elements).
#define TX_QUEUE_MAX_SIZE               (8u)

/// The size of the data array that holds the queue element data in the transmit
/// queue.
#define TX_QUEUE_DATA_SIZE              (600u)


// === TYPE DEFINES ============================================================

/// Defines the type of transfer mode for the read/write transaction on the I2C
/// bus.
typedef union TransferMode
{
    struct
    {
        /// Start the transaction with restart instead of start.
        bool repeatStart : 1;
    
        /// Do not complete the transfer with a stop.
        bool noStop : 1;
    };
    
    /// The value of the transfer mode mask.
    uint8_t value;
    
} TransferMode;


/// Pre-defined 7-bit addresses of slave devices on the I2C bus.
typedef enum SlaveAddress
{
    /// Default 7-bit I2C address of the main application for UICO generation 2
    /// duraTOUCH MCU's (dT1xx, dT2xx, and dT4xx).
    SlaveAddress_App                    = 0x48,
    
    /// Default 7-bit I2C address of the bootloader for UICO generation 2
    /// duraTOUCH MCU's (dT1xx, dT2xx, and dT4xx).
    SlaveAddress_Bootloader             = 0x58,
    
} SlaveAddress;


/// Definition of the transmit queue data offsets.
typedef enum TxQueueDataOffset
{
    /// The I2C address.
    TxQueueDataOffset_Address           = 0u,
    
    /// The start of the data payload.
    TxQueueDataOffset_Data              = 1u,
    
} TxQueueDataOffset;


/// Definition of the duraTOUCH application I2C communication receive packet
/// offsets.
typedef enum AppRxPacketOffset
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
typedef enum AppTxPacketOffset
{
    /// Length byte offset.
    AppTxPacketOffset_BufferOffset      = 0u,
    
    /// Data payload offset.
    AppTxPacketOffset_Data              = 1u,
    
} AppTxPacketOffset;


/// Definition of the duraTOUCH application memory buffer offset used for
/// setting the app buffer offset in tramsit packets.
typedef enum AppBufferOffset
{
    /// Command buffer offset; used to write a command.
    AppBufferOffset_Command             = 0x00,
    
    /// Response buffer offset; used to read a command.
    AppBufferOffset_Response            = 0x20,
    
} AppBufferOffset;


/// States used by the app receive state machine to handle the different steps
/// in processing the responses.
typedef enum AppRxState
{
    /// The initial state.
    AppRxState_Start,
    
    /// App needs to be switched to the response buffer being active.
    AppRxState_SwitchToResponseBuffer,
    
    /// Read the length of the response.
    AppRxState_ReadLength,
    
    /// Read the remaining data payload state.
    AppRxState_ReadDataPayload,
    
    /// Complete the read operation by sending a stop.
    AppRxState_StopRead,
    
    /// Clear the IRQ.
    AppRxState_ClearIrq,
    
    /// Receive completed.
    AppRxState_Complete,
    
    /// An error occured and the transaction couldn't be completed.
    AppRxState_Error,
    
} AppRxState;


/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap
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
static uint8_t g_slaveAddress = SlaveAddress_App;

/// Flag indicating if the IRQ triggerd and a receive is pending consumption.
static volatile bool g_rxPending = false;

/// Flag indicating we're in the response buffer is active for the slave app.
static bool g_slaveAppResponseActive = false;

/// The receive callback function.
static I2cGen2_RxCallback g_rxCallback = NULL;

/// The error callback function.
static I2cGen2_ErrorCallback g_errorCallback = NULL;

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
    uint32_t status = COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    g_lastDriverStatus = status;
    return ((status & COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_INP)) == 0);
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


/// Calculates how much to extend a timeout based on having to perform some
/// additional I2C transactions. This function assumes that the SCL is running
/// at approximately 100 kHz (bit-rate).
/// Note that this is an approximation because we perform the calculation in
/// microseconds but we return the value in milliseconds. Instead of using a
/// 10^3 (1000) conversion factor, we use a 2^10 (1024) conversion factor and
/// round up with an adjustment.
/// @param[in]  transactionSize The size in bytes of the additional transaction.
/// @return The additional time to add to the timeout in milliseconds.
static uint32_t findExtendedTimeoutMS(uint16_t transactionSize)
{
    static uint32_t const WordSize = 9u;
    static uint32_t const PeriodUS = 10u;
    static uint32_t const Shift = 10u;
    static uint32_t const Adjustment = 1u;
    
    uint32_t extendedTimeoutMS = transactionSize * WordSize * PeriodUS;
    extendedTimeoutMS = (extendedTimeoutMS >> Shift) + Adjustment;
    return extendedTimeoutMS;
}


/// Read data from a slave device on the I2C bus.
/// @param[in]  address
/// @param[out] data    Data buffer to copy the read data to.
/// @param[in]  size    The number of bytes to read; it's assumed that data is
///                     at least this length.
/// @param[in]  mode    TransferMode settings.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status read(uint8_t address, uint8_t data[], uint16_t size, TransferMode mode)
{
    I2cGen2Status status = { false };
    uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, mode.value);
    if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
    {
        status.driverError = true;
        if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
            status.nak = true;
    }
    g_lastDriverStatus = driverStatus;
    return status;
}


/// Write data to the slave device on the I2C bus.
/// @param[in]  address
/// @param[out] data    Data buffer to copy the read data to.
/// @param[in]  size    The number of bytes to read; it's assumed that data is
///                     at least this length.
/// @param[in]  mode    TransferMode settings.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status write(uint8_t address, uint8_t data[], uint16_t size, TransferMode mode)
{
    I2cGen2Status status = { false };
    uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, data, size, mode.value);
    if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
    {
        status.driverError = true;
        if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
            status.nak = true;
    }
    g_lastDriverStatus = driverStatus;
    return status;
}


/// Sends the stop condition on the I2C bus. Only call this if a previous read
/// or write call was called with the TransferMode.noStop flag was set to true.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status sendStop(void)
{
    I2cGen2Status status = { false };
    uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
    if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
    {
        status.driverError = true;
        if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
            status.nak = true;
        if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_TIMEOUT)) > 0)
            status.timedOut = true;
    }
    g_lastDriverStatus = driverStatus;
    return status;
}


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status resetIrq(void)
{
    uint8_t clear[] = { AppBufferOffset_Response, 0 };
    TransferMode mode = { { false, false} };
    I2cGen2Status status = write(g_slaveAddress, clear, sizeof(clear), mode);
    return status;
}


/// Changes the slave app to response buffer active state so responses can be
/// properly read from the slave device.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status changeSlaveAppToResponseBuffer(void)
{
    I2cGen2Status status = { false };
    uint8_t responseMessage[] = { AppBufferOffset_Response };
    TransferMode mode = { { false, false } };
    status = write(g_slaveAddress, responseMessage, sizeof(responseMessage), mode);
    if (!status.errorOccurred)
        g_slaveAppResponseActive = true;
    return status;
}


/// State machine to process the IRQ indicating that the slave app has data
/// ready to be received by the host.
/// @param[in]  timeoutMS   The amount of time the process can occur before it
///                         times out and must finish. If 0, then there's no
///                         timeout and the function blocks until all pending
///                         actions are completed.
/// @return The number of bytes that were processed. If 0, then no bytes were
///         pending to receive. If -1, an error occurred: there was data pending
///         but it could not be read because the bus was busy.
static int processAppRxStateMachine(uint32_t timeoutMS)
{
    Alarm alarm;
    if (timeoutMS > 0)
        alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
    else
        alarm_disarm(&alarm);
        
    int length = 0;
    uint8_t payloadLength = 0;
    I2cGen2Status status = { false };
    AppRxState state = AppRxState_Start;    
    while (state != AppRxState_Complete)
    {
        if (alarm_hasElapsed(&alarm))
        {
            status.timedOut = true;
            break;
        }
        
        switch (state)
        {
            case AppRxState_Start:
            {
                length = 0;
                if (g_slaveAppResponseActive)
                    state = AppRxState_ReadLength;
                else
                    state = AppRxState_SwitchToResponseBuffer;
                break;
            }
            
            case AppRxState_SwitchToResponseBuffer:
            {
                if (isBusReady())
                {
                    status = changeSlaveAppToResponseBuffer();
                    if (!status.errorOccurred)
                        state = AppRxState_ReadLength;
                    else
                        state = AppRxState_Error;
                }
                break;
            }
            
            case AppRxState_ReadLength:
            {
                if (isBusReady())
                {
                    TransferMode mode = { { false, true } };
                    status = read(g_slaveAddress, g_heap->rxBuffer, G_AppRxPacketLengthSize, mode);
                    if (!status.errorOccurred)
                    {
                        payloadLength = g_heap->rxBuffer[AppRxPacketOffset_Length];
                        if (isAppPacketLengthValid(payloadLength))
                        {
                            length += G_AppRxPacketLengthSize;
                            if (length <= 0)
                            {
                                if (g_rxCallback != NULL)
                                    g_rxCallback(g_heap->rxBuffer, (uint16_t)length);
                                state = AppRxState_StopRead;
                            }
                            else
                            {
                                alarm_snooze(&alarm, findExtendedTimeoutMS(payloadLength));
                                state = AppRxState_ReadDataPayload;
                            }
                        }
                        else
                        {
                            status.invalidRead = true;
                            state = AppRxState_Error;
                        }
                    }
                    else
                        state = AppRxState_Error;
                }
                break;
            }
            
            case AppRxState_ReadDataPayload:
            {
                if (isBusReady())
                {
                    TransferMode mode = { { true, false } };
                    status = read(g_slaveAddress, &g_heap->rxBuffer[AppRxPacketOffset_Data], payloadLength, mode);
                    if (!status.errorOccurred)
                    {
                        length += payloadLength;
                        if (g_rxCallback != NULL)
                            g_rxCallback(g_heap->rxBuffer, (uint16_t)length);
                        state = AppRxState_Complete;
                    }
                    else
                        state = AppRxState_Error;
                }
                break;
            }
            
            case AppRxState_Complete:
            {
                break;
            }
            
            case AppRxState_StopRead:
            {
                if (isBusReady())
                {
                    status = sendStop();
                    if (!status.errorOccurred)
                        state = AppRxState_ClearIrq;
                    else
                        state = AppRxState_Error;
                }
                break;
            }
            
            case AppRxState_ClearIrq:
            {
                if (isBusReady())
                {
                    status = resetIrq();
                    if (!status.errorOccurred)
                        state = AppRxState_Complete;
                    else
                        state = AppRxState_Error;
                }
                break;
            }
            
            case AppRxState_Error:
            {
                if (g_errorCallback != NULL)
                    g_errorCallback(status);
                break;
            }
            
            default:
            {
            }
        }
    }
    return length;
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
    g_slaveAppResponseActive = false;
    
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
        g_slaveAppResponseActive = false;
    }
    return allocatedSize;
}
 

void i2cGen2_deactivate(void)
{
    g_heap = NULL;
    g_slaveAppResponseActive = false;
}


void i2cGen2_setSlaveAddress(uint8_t address)
{
    g_slaveAddress = address;
    g_slaveAppResponseActive = false;
}


void i2cGen2_resetSlaveAddress(void)
{
    g_slaveAddress = SlaveAddress_App;
    g_slaveAppResponseActive = false;
}

    
void i2cGen2_registerRxCallback(I2cGen2_RxCallback callback)
{
    if (callback != NULL)
        g_rxCallback = callback;
}


void i2cGen2_registerErrorCallback(I2cGen2_ErrorCallback callback)
{
    if (callback != NULL)
        g_errorCallback = callback;
}


int i2cGen2_processRx(uint32_t timeoutMS)
{
    int length = 0;
    if ((g_heap != NULL) && g_rxPending && isIrqAsserted())
    {
        processAppRxStateMachine(timeoutMS);
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
        if (count > 0)
            debug_printf("[I:Tx]=%u\n", count);
    }
    else
        count = -1;
    return count;
}


I2cGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size)
{
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            debug_printf("[I:R]");
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                    status.driverError = true;
                g_lastDriverStatus = driverStatus;
            }
            else
                status.timedOut = true;
            if (status.errorOccurred)
                debug_printf("%x\n", g_lastDriverStatus);
            else
                debug_printf("\n");
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2cGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            debug_printf("[I:W]");
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, data, size, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                    status.driverError = true;
                g_lastDriverStatus = driverStatus;
                if (address == g_slaveAddress)
                    g_slaveAppResponseActive = (data[0] == AppBufferOffset_Response);
            }
            else
                status.timedOut = true;
            if (status.errorOccurred)
                debug_printf("%x\n", g_lastDriverStatus);
            else
                debug_printf("\n");
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    return status;
}


I2cGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size)
{
    static uint8_t const MinSize = 2u;
    static uint8_t const AddressOffset = 0u;
    static uint8_t const DataOffset = 1u;
    
    I2cGen2Status status = { false };
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


I2cGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size)
{
    I2cGen2Status status = { false };
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


I2cGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size)
{
    I2cGen2Status status = { false };
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


I2cGen2Status i2cGen2_ack(uint8_t address, uint32_t timeoutMS)
{
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
        else
            alarm_disarm(&alarm);
        
        debug_printf("[I:A]");
        while (true)
        {
            if (alarm_hasElapsed(&alarm))
            {
                status.timedOut = true;
                break;
            }
            
            // Scratch buffer; used so that the I2C read function has a valid
            // non-NULL pointer for reading 0 bytes.
            uint8_t scratch;
            if (isBusReady())
            {
                uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, &scratch, 0, COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
                if (driverStatus != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
                {
                    status.driverError = true;
                    if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
                        status.nak = true;
                }
                g_lastDriverStatus = driverStatus;
            }
            if (status.errorOccurred)
                debug_printf("%x", g_lastDriverStatus);
        }
        debug_printf("\n");
    }
    else
        status.deactivated = true;
    return status;
}


I2cGen2Status i2cGen2_ackApp(uint32_t timeoutMS)
{
    return i2cGen2_ack(g_slaveAddress, timeoutMS);
}


/* [] END OF FILE */
