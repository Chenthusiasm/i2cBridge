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
    /// The initial reset state.
    AppRxState_Reset,
    
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
/// @return If the slave app is in the response buffer active state.
static bool changeSlaveAppToResponseBuffer(void)
{
    if (g_slaveAppResponseActive)
    {
        uint8_t responseMessage[] = { AppBufferOffset_Response };
        uint32_t driverStatus = COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(g_slaveAddress, responseMessage, sizeof(responseMessage), COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER));
        g_slaveAppResponseActive = (driverStatus == COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR));
        g_lastDriverStatus = driverStatus;
    }
    return g_slaveAppResponseActive;
}


/// State machine to process the IRQ indicating that the slave app has data
/// ready to be received by the host.
/// @param[in/out]  state   The current state. Will update to the next state.
/// @return The I2cGen2Status processing the current state.
static I2cGen2Status processAppRxStateMachine(AppRxState* state)
{
    static int length = 0;
    I2cGen2Status status = { false };
    
    
    switch (*state)
    {
        case AppRxState_Reset:
        {
            length = 0;
            if (g_slaveAppResponseActive)
                *state = AppRxState_ReadLength;
            else
                *state = AppRxState_SwitchToResponseBuffer;
            break;
        }
        
        case AppRxState_SwitchToResponseBuffer:
        {
            if (isBusReady())
                changeSlaveAppToResponseBuffer();
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
                    if (g_heap->rxBuffer[AppRxPacketOffset_Length] < G_InvalidRxAppPacketLength)
                    {
                        length += G_AppRxPacketLengthSize;
                        if (length == 0)
                            *state = AppRxState_StopRead;
                        else
                            *state = AppRxState_ReadDataPayload;
                    }
                    else
                    {
                        status.invalidRead = true;
                        *state = AppRxState_Error;
                    }
                }
                else
                    *state = AppRxState_Error;
            }
            break;
        }
        
        case AppRxState_ReadDataPayload:
        {
            if (isBusReady())
            {
                TransferMode mode = { { true, false } };
                uint8_t readLength = g_heap->rxBuffer[AppRxPacketOffset_Length];
                status = read(g_slaveAddress, &g_heap->rxBuffer[AppRxPacketOffset_Data], readLength, mode);
                if (!status.errorOccurred)
                {
                    length += readLength;
                    *state = AppRxState_ClearIrq;
                }
                else
                    *state = AppRxState_Error;
            }
            break;
        }
        
        case AppRxState_StopRead:
        {
            if (isBusReady())
                COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
            break;
        }
        
        case AppRxState_ClearIrq:
        {
            break;
        }
        
        case AppRxState_Complete:
        {
            break;
        }
        
        case AppRxState_Error:
        {
            break;
        }
        
        default:
        {
        }
    }
    return status;
}

//            debug_printf("a");
//            if (isBusReady())
//            {
//                debug_printf("b");
//                if (!g_slaveAppResponseActive)
//                {
//                    debug_printf("c");
//                    changeSlaveAppToResponseBuffer();
//                }
//                
//                CyDelay(2);
//                if (COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(g_slaveAddress, g_heap->rxBuffer, G_AppRxPacketLengthSize, COMPONENT(SLAVE_I2C, I2C_MODE_NO_STOP)) == COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))
//                {
//                    debug_printf("d");
//                    length += (int)G_AppRxPacketLengthSize;
//                    uint8_t dataLength = g_heap->rxBuffer[AppRxPacketOffset_Length];
//                    if (isAppPacketLengthValid(dataLength))
//                    {
//                        debug_printf("e");
//                        if (dataLength > 0)
//                        {
//                            COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(g_slaveAddress, &g_heap->rxBuffer[AppRxPacketOffset_Data], dataLength, COMPONENT(SLAVE_I2C, I2C_MODE_REPEAT_START));
//                            debug_printf("f");
//                        }
//                        else
//                        {
//                            COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
//                            debug_printf("g");
//                        }
//                        length += (int)dataLength;
//                        if (g_rxCallback != NULL)
//                            g_rxCallback(g_heap->rxBuffer, (uint16_t)length);
//                    }
//                    else
//                    {
//                        COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
//                        length = -1;
//                        debug_printf("h", 5);
//                    }
//                }
//                else
//                    debug_printf("i");
//                resetIrq();
//                g_rxPending = false;
//            }
//            else
//                length = -1;
//            debug_printf("\n");


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

    
void i2cGen2_registerRxCallback(I2cGen2_RxCallback pCallback)
{
    if (pCallback != NULL)
        g_rxCallback = pCallback;
}


int i2cGen2_processRx(uint32_t timeoutMS)
{
    int length = 0;
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_SingleNotification);
        else
            alarm_disarm(&alarm);
            
        while (g_rxPending && isIrqAsserted())
        {
            if (alarm_hasElapsed(&alarm))
            {
                length = -1;
            }

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
                status.busBusy = true;
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
                status.busBusy = true;
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
                status.busBusy = true;
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
