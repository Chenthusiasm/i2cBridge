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

#include "uartFrameProtocol.h"

#include <stdio.h>
#include <string.h>

#include "hwSystemTime.h"
#include "project.h"
#include "queue.h"



// === TYPEDEFINES =============================================================

/// Defines the different states of the protocol state machine.
typedef enum RxState_
{
    /// Outside of a valid data frame, do not process this data.
    RxState_OutOfFrame,
    
    /// Inside a valid data frame, the data needs to be processed.
    RxState_InFrame,
    
    /// An escape character occurred, interpret the next byte as data instead of
    /// a control byte.
    RxState_EscapeCharacter,
    
} RxState;


/// Defines the different control bytes in the protocol which helps define a
/// data frame.
typedef enum ControlByte_
{
    /// Start of a valid data frame.
    ControlByte_StartFrame              = 0xaa,
    
    /// End of a valid data frame.
    ControlByte_EndFrame                = ControlByte_StartFrame,
    
    /// Escape character. Interpret the next byte as data instead of a control
    /// byte.
    ControlByte_Escape                  = 0x55,
    
} ControlByte;


typedef enum BridgeCommand_
{
    /// No command.
    BridgeCommand_None                  = 0x00,
    
    /// Host to bridge ACK over UART.
    BridgeCommand_ACK                   = 'A',
    
    /// I2C slave error.
    BridgeCommand_SlaveError            = 'E',
    
    /// Access the I2C slave address.
    BridgeCommand_SlaveAddress          = 'I',
    
    /// Bridge to I2C slave NACK over I2C.
    BridgeAddress_SlaveNACK             = 'N',
    
    /// Bridge I2C read from I2C slave.
    BridgeCommand_SlaveRead             = 'R',
    
    /// I2C communication timeout between bridge and I2C slave.
    BridgeCommand_SlaveTimeout          = 'T',
    
    /// Configures bridge to slave update mode.
    BridgeCommand_SlaveUpdate           = 'U',
    
    /// Bridge version information.
    BridgeCommand_Version               = 'V',
    
    /// Bridge I2C write to I2C slave.
    BridgeCommand_SlaveWrite            = 'W',
    
    /// Bridge to I2C slave ACK over I2C.
    BridgeCommand_SlaveACK              = 'a',
    
    /// Bridge reset.
    BridgeCommand_Reset                 = 'r',
    
} BridgeCommand;


/// Enumeration that defines the offsets of different types of bytes within the
/// UICO I2C protocol.
typedef enum PacketOffset_
{
    /// Offset in the data frame for the bridge command.
    PacketOffset_BridgeCommand          = 0u,
    
    /// Offset in the data frome for the data payload.
    PacketOffset_BridgeData             = 1u,
} PacketOffset;


/// Type flags that describe the type of packet.
typedef struct Flags_
{
    // Packet contains a command.
    bool command : 1;
    
    // Packet contains data.
    bool data : 1;
    
} Flags;


// === DEFINES =================================================================

/// The amount of time between receipts of bytes before we automatically reset
/// the receive state machine.
#define RX_RESET_TIMEOUT_MS                 (2000u)

/// The size of the processed and pending receive buffer for UART transactions.
#define RX_BUFFER_SIZE                      (300u)

/// The max size of the transmit queue (the max number of queue elements).
#define TX_QUEUE_MAX_SIZE                   (16u)

/// The size of the data array that holds the queue element data in the transmit
/// queue.
#define TX_QUEUE_DATA_SIZE                  (500u)


// === GLOBALS =================================================================

/// The current state in the protocol state machine for receive processing.
/// frame.
static volatile RxState g_rxState = RxState_OutOfFrame;

/// The post-processed decoded receive buffer.  All decoded received data is
/// stored in this buffer until a full data frame is processed.
static uint8_t g_decodedRxBuffer[RX_BUFFER_SIZE];

/// Flag indicating if there's a valid packet in the decoded receive buffer
/// pending to be processed.
static volatile bool g_decodedRxPacketPending = false;

/// The current index of data in the processed received data buffer.  We need
/// this index to persist because we may receive only partial packets; this
/// allows for continuous processing of received data.
static volatile uint16_t g_decodedRxIndex = 0;

/// The last time data was received in milliseconds.
static volatile uint32_t g_lastRxTimeMS = 0;

/// Callback function that is invoked when data is received out of the frame
/// state machine.
static UartFrameProtocol_RxOutOfFrameCallback g_rxOutOfFrameCallback = NULL;

/// Callback function that is invoked when data is received but the receive
/// buffer is not large enough to store it so the data overflowed.
static UartFrameProtocol_RxFrameOverflowCallback g_rxFrameOverflowCallback = NULL;

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
};

/// The type flags of the data that is waiting to be enqueued into the transmit
/// queue. This must be set prior to enqueueing data into the transmit queue.
static Flags g_pendingTxEnqueueFlags = { false, false };

/// The command associated with the data that is watiting to be enqueued into
/// the transmit queue. This must be set prior to enqueueing data into the
/// transmit queue.
static BridgeCommand g_pendingTxEnqueueCommand = BridgeCommand_None;


// === PRIVATE FUNCTIONS =======================================================

/// Sets all the global variables pertaining to the decoded receive buffer to
/// an initial state.
static void resetDecodedRxBufferParameters(void)
{
    g_decodedRxPacketPending = false;
    g_decodedRxIndex = 0;
    g_lastRxTimeMS = hwSystemTime_getCurrentMS();
}


/// This function checks if the byte is an end frame character.  This is mainly
/// needed when parsing/processing data that has been received over UART and is
/// in the UICO UART protocol format.
/// @param[in]  data    The data byte to check to see if it's an end frame
///                     character.
/// @return If the data byte is an end frame character (true) or not (false).
static bool isEndFrameCharacter(uint8_t data)
{
    return (data == ControlByte_EndFrame);
}


/// This function checks if the byte is an escape character.  This is mainly
/// needed when parsing/processing data that has been received over UART and is
/// in the UICO UART protocol format.
/// @param[in]  data    The data byte to check to see if it's an escape
///                     character.
/// @return If the data byte is an escape character (true) or not (false).
static bool isEscapeCharacter(uint8_t data)
{
    return (data == ControlByte_Escape);
}


/// This function checks if the byte requires an escape character.  This is
/// mainly needed when setting up data to be transmitted and we need to format
/// the data packet to fit the UICO UART protocol.
/// @param[in]  data    The data byte to check to see if it requires an escape
///                     character.
/// @return If the data byte requires an escape character (true) or not (false).
static bool requiresEscapeCharacter(uint8_t data)
{
    return ((data == ControlByte_StartFrame) ||
            (data == ControlByte_EndFrame) ||
            (data == ControlByte_Escape));
}


/// Handle any byte in the processed via the receive state machine that would
/// overflow because it doesn't fit in the receive buffer.
/// @param[in]  data    The data byte that overflowed (didn't fit in the receive
///                     buffer).
static void handleRxFrameOverflow(uint8_t data)
{
    if (g_rxFrameOverflowCallback != NULL)
        g_rxFrameOverflowCallback(data);
}


/// Resets the variables associated with the pending transmit enqueue.
void resetPendingTxEnqueue(void)
{
    g_pendingTxEnqueueCommand = BridgeCommand_None;
    static Flags const DefaultFlags = { false, false };
    g_pendingTxEnqueueFlags = DefaultFlags;
}


/// Generates the formatted packet that defines the UART frame protocol. The
/// formatted packet will have the 0xaa frame characters and 0x55 escape
/// characters as necessary along with command byte if the packet pertains to
/// a bridge command. Note that the packet's type flags and command bytes must
/// be primed before invoking this function.
/// @param[in]  source      The source buffer.
/// @param[in]  sourceSize  The number of bytes in the source.
/// @param[out] target      The target buffer (where the formatted data is
///                         stored).
/// @param[in]  targetSize  The number of bytes available in the target.
/// @return The number of bytes in the target buffer or the number of bytes
///         to transmit.  If 0, then the source buffer was either invalid or
///         there's not enough bytes in target buffer to store the formatted
///         data.
uint16_t encodeData(uint8_t target[], uint16_t targetSize, uint8_t const source[], uint16_t sourceSize)
{
    uint16_t t = 0;
    
    if ((targetSize > 0) && (target != NULL))
    {
        // Always put the start frame control byte in the beginning.
        target[t++] = ControlByte_StartFrame;
        
        bool processPendingData = true;
        if (g_pendingTxEnqueueFlags.command && (g_pendingTxEnqueueCommand != BridgeCommand_None))
        {
            static uint8_t const CommandSize = 3u;
            if ((t + CommandSize) > targetSize)
            {
                processPendingData = false;
                t = 0;
            }
            else
            {
                target[t++] = ControlByte_Escape;
                target[t++] = ControlByte_Escape;
                target[t++] = g_pendingTxEnqueueCommand;
            }
        }
        
        if (g_pendingTxEnqueueFlags.data && processPendingData && (sourceSize > 0) && (source != NULL))
        {
            // Iterate through the source buffer and copy it into transmit buffer.
            for (uint32_t s = 0; s < sourceSize; ++s)
            {
                if (t > targetSize)
                {
                    t = 0;
                    break;
                }
                uint8_t data = source[s];
                
                // Check to see if an escape character is needed.
                if (requiresEscapeCharacter(data))
                {
                    target[t++] = ControlByte_Escape;
                    if (t > targetSize)
                    {
                        t = 0;
                        break;
                    }
                }
                target[t++] = data;
            }
        }
        
        // Always put the end frame control byte in the end.
        if ((t > 0) && (t < targetSize))
            target[t++] = ControlByte_EndFrame;
        else
            t = 0;
    }
    resetPendingTxEnqueue();
    return t;
}


/// Generates the formatted packet that defines the UART frame protocol. The
/// formatted packet will have the 0xaa frame characters and 0x55 escape
/// characters as necessary along with command byte if the packet pertains to
/// a bridge command. Note that the packet's type flags and command bytes must
/// be primed before invoking this function.
/// @param[in]  source      The source buffer.
/// @param[in]  sourceSize  The number of bytes in the source.
/// @param[out] target      The target buffer (where the formatted data is
///                         stored).
/// @param[in]  targetSize  The number of bytes available in the target.
/// @return The number of bytes in the target buffer or the number of bytes
///         to transmit.  If 0, then the source buffer was either invalid or
///         there's not enough bytes in target buffer to store the formatted
///         data.
uint16_t __attribute__((unused)) encodeTxData(uint8_t target[], uint16_t targetSize, BridgeCommand command, Flags flags, uint8_t const source[], uint16_t sourceSize)
{
    g_pendingTxEnqueueCommand = command;
    g_pendingTxEnqueueFlags = flags;
    return encodeData(target, targetSize, source, sourceSize);
}


/// Enqueue command and any associated data into the transmit queue.
/// @param[in]  command The command associated with the transmit packet.
/// @param[in]  data    The data to enqueue. If this is NULL, then the data flag
///                     will not be set.
/// @param[in]  size    The size of the data. If the value is 0, then the data
///                     flag will not be set.
/// @return If the command and associated data was successfully enqueued.
bool txEnqueueCommand(BridgeCommand command, uint8_t const data[], uint16_t size)
{
    bool status = false;
    if (!queue_isFull(&g_txQueue) && (command != BridgeCommand_None))
    {
        g_pendingTxEnqueueCommand = command;
        g_pendingTxEnqueueFlags.command = false;
        g_pendingTxEnqueueFlags.data = ((data != NULL) && (size > 0));
        queue_enqueue(&g_txQueue, data, size); 
    }
    return status;
}


/// Processes the processed UART data (where the frame and escape characters are
/// removed).
/// @return If the UART command was successfully processed or not.
static bool processCompleteRxPacket(void)
{
    bool status = false;
    if ((g_decodedRxPacketPending) && (g_decodedRxIndex > PacketOffset_BridgeCommand))
    {
        uint8_t command = g_decodedRxBuffer[PacketOffset_BridgeCommand];
        switch (command)
        {
            case BridgeCommand_ACK:
            {
                txEnqueueCommand(BridgeCommand_ACK, NULL, 0);
                break;
            }
            
            case BridgeCommand_SlaveError:
            {
                // @TODO Send last slave device error.
                break;
            }
            
            case BridgeCommand_SlaveAddress:
            {
                // @TODO Modify the slave address.
                break;
            }
            
            case BridgeAddress_SlaveNACK:
            {
                break;
            }
            
            case BridgeCommand_SlaveRead:
            {
                break;
            }
            
            case BridgeCommand_SlaveTimeout:
            {
                break;
            }
            
            case BridgeCommand_SlaveUpdate:
            {
                break;
            }
            
            case BridgeCommand_Version:
            {
                break;
            }
            
            case BridgeCommand_SlaveWrite:
            {
                break;
            }
            
            case BridgeCommand_SlaveACK:
            {
                break;
            }
            
            case BridgeCommand_Reset:
            {
                CySoftwareReset();
                break;
            }
            
            default:
            {
                break;
            }
        }
        g_decodedRxPacketPending = false;
    }
    return status;
}

static bool processReceivedByte(uint8_t data)
{
    bool status = true;
    switch (g_rxState)
    {
        case RxState_OutOfFrame:
        {
            if (data == ControlByte_StartFrame)
            {                        
                resetDecodedRxBufferParameters();
                g_rxState = RxState_InFrame;
            }
            else
            {
                if (g_rxOutOfFrameCallback != NULL)
                    g_rxOutOfFrameCallback(data);
                status = false;
            }
            break;
        }
        
        case RxState_InFrame:
        {
            if (isEscapeCharacter(data))
                g_rxState = RxState_EscapeCharacter;
            else if (isEndFrameCharacter(data))
            {
                g_decodedRxPacketPending = true;
                g_rxState = RxState_OutOfFrame;
            }
            else
            {
                // Make sure the data will fit in the buffer before adding.
                if (g_decodedRxIndex < RX_BUFFER_SIZE)
                    g_decodedRxBuffer[g_decodedRxIndex++] = data;
                else
                {
                    handleRxFrameOverflow(data);
                    status = false;
                }
            }
            break;
        }
        
        case RxState_EscapeCharacter:
        {
            // Make sure the data will fit in the buffer before adding.
            if (g_decodedRxIndex < RX_BUFFER_SIZE)
            {
                g_decodedRxBuffer[g_decodedRxIndex++] = data;
                g_rxState = RxState_InFrame;
            }
            else
            {
                handleRxFrameOverflow(data);
                status = false;
            }
            break;
        }
        
        default:
        {
            // We should never get into this state, if we do, something
            // wrong happened.  Potentially do some error handling.
            
            // Reset to the default state: out of frame.
            g_rxState = RxState_OutOfFrame;
            status = false;
            break;
        }
    }
    return status;
}


/// Processes the receive buffer with the intent of parsing out valid frames of
/// data.
/// @param[in]  source          The buffer to process and parse to find full
///                             valid frames of data.
/// @param[in]  sourceSize      The size of the source buffer in bytes.
/// @param[in]  sourceOffset    The offset to start parsing.
/// @return The number of bytes that were processed.
static uint16_t processReceivedData(uint8_t const source[], uint16_t sourceSize, uint16_t sourceOffset)
{
    // Track the number of bytes that was processed.
    uint32_t size = 0;
    
    // Iterate through all the received bytes via UART.
    bool exit = false;
    for (uint32_t i = sourceOffset; i < sourceSize; ++i)
    {
        uint8_t data = source[i];
        ++size;
        
        processReceivedByte(data);
        if (g_decodedRxPacketPending)
            break;
    }
    return size;
}


// === ISR =====================================================================

static void isr(void)
{
    uint32_t source = hostUART_GetRxInterruptSource();
    if ((source & hostUART_INTR_RX_NOT_EMPTY) != 0)
    {
        uint32_t data = hostUART_UartGetByte();
        if (data > 0xff)
        {
            // @TODO Error handling.
        }
        else
            processReceivedByte(data);
        hostUART_ClearRxInterruptSource(hostUART_INTR_RX_NOT_EMPTY);
    }
    else if ((source & hostUART_INTR_RX_FRAME_ERROR) != 0)
    {
        // Do some special handling for a frame error; possibly determine baud
        // rate.
        hostUART_ClearRxInterruptSource(hostUART_INTR_RX_FRAME_ERROR);
    }
    hostUART_ClearPendingInt();
}


// === PUBLIC FUNCTIONS ========================================================

void uartFrameProtocol_init(void)
{
    // Configure the receive variables.
    g_rxState = RxState_OutOfFrame;
    resetDecodedRxBufferParameters();
    
    // Configures the transmit variables.
    queue_registerEnqueueCallback(&g_txQueue, encodeData);
    queue_empty(&g_txQueue);
    resetPendingTxEnqueue();
    
    // Setup the UART hardware.
    hostUART_SetCustomInterruptHandler(isr);
    hostUART_Start();
}


void uartFrameProtocol_registerRxOutOfFrameCallback(UartFrameProtocol_RxOutOfFrameCallback callback)
{
    if (callback != NULL)
        g_rxOutOfFrameCallback = callback;
}


void uartFrameProtocol_registerRxFrameOverflowCallback(UartFrameProtocol_RxFrameOverflowCallback callback)
{
    if (callback != NULL)
        g_rxFrameOverflowCallback = callback;
}


bool uartFrameProtocol_isTxQueueEmpty(void)
{
    return (queue_isEmpty(&g_txQueue));
}


uint16_t uartFrameProtocol_processRxData(uint8_t const data[], uint16_t size)
{
    uint32_t receiveTimeMS = (uint32_t)hwSystemTime_getCurrentMS();
    
    // Check if we haven't received data in a while.
    if ((uint32_t)(receiveTimeMS - g_lastRxTimeMS) > RX_RESET_TIMEOUT_MS)
        uartFrameProtocol_init();
    
    uint16_t processSize = 0;
    if ((data != NULL) && (size > 0))
    {
        uint16_t offset = 0;
        while (offset < size)
        {
            offset += processReceivedData(data, size, offset);
            
            // Check if there's a valid packet to process.
            if (g_decodedRxPacketPending && (g_decodedRxIndex > 0))
            {
                processCompleteRxPacket();
                resetDecodedRxBufferParameters();
            }
        }
    }
    
    // Update the last receive time.
    g_lastRxTimeMS = receiveTimeMS;
    
    return processSize;
}


uint16_t uartFrameProtocol_processTxQueue(void)
{
    uint16_t size = 0;
    if (!queue_isEmpty(&g_txQueue))
    {
        uint8_t* data;
        size = queue_dequeue(&g_txQueue, &data);
        for (uint32_t i = 0; i < size; ++i)
            hostUART_UartPutChar(data[i]);
    }
    return size;
}


bool uartFrameProtocol_txEnqueueData(uint8_t const data[], uint16_t size)
{
    bool status = false;
    if (!queue_isFull(&g_txQueue))
    {
        g_pendingTxEnqueueCommand = BridgeCommand_None;
        g_pendingTxEnqueueFlags.command = false;
        g_pendingTxEnqueueFlags.data = true;
        queue_enqueue(&g_txQueue, data, size); 
    }
    return status;
}


/* [] END OF FILE */
