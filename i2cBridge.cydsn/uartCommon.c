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

#include "uartCommon.h"

#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "debug.h"
#include "error.h"
#include "hwSystemTime.h"
#include "i2cTouch.h"
#include "project.h"
#include "queue.h"
#include "utility.h"
#include "version.h"


// === DEFINES =================================================================

/// Name of the host UART component.
#define HOST_UART                       hostUart_

/// The max size of the receive queue (the max number of queue elements).
#define RX_QUEUE_MAX_SIZE               (8u)

/// The size of the data array that holds the queue element data in the receive
/// queue.
#define RX_QUEUE_DATA_SIZE              (600u)

/// The max size of the transmit queue (the max number of queue elements).
#define TX_QUEUE_MAX_SIZE               (8u)

/// The size of the data array that holds the queue element data in the transmit
/// queue.
#define TX_QUEUE_DATA_SIZE              (800u)

/// The size of the data array that holds the queue element data in the receive
/// queue when in update mode.
/// Note: in the previous implementation of the bridge, the Rx FIFO was
/// allocated 2052 bytes; this should be larger than that.
#define UPDATER_RX_QUEUE_DATA_SIZE      (2100u)

/// The size of the data array that holds the queue element data in the transmit
/// queue. This should be smaller than the receive queue data size to account
/// for the change in the receive/transmit balance.
#define UPDATER_TX_QUEUE_DATA_SIZE      (100u)


// === TYPE DEFINES ============================================================

/// Defines the different states of the protocol state machine.
typedef enum RxState
{
    /// Outside of a valid data frame, do not process this data.
    RxState_OutOfFrame,
    
    /// Inside a valid data frame, the data needs to be processed.
    RxState_InFrame,
    
    /// An escape character occurred, interpret the next byte as data instead of
    /// a control byte.
    RxState_EscapeCharacter,
    
    /// Process the update packet's size (high byte).
    RxState_UpdatePacketSizeHiByte,
    
    /// Process the update packet's size (low byte).
    RxState_UpdatePacketSizeLoByte,
    
    /// Process the update packet's data (data to be sent to the slave device).
    RxState_UpdatePacketData,
    
} RxState;


/// Defines the different control bytes in the protocol which helps define a
/// data frame.
typedef enum ControlByte
{
    /// Start of a valid data frame.
    ControlByte_StartFrame              = 0xaa,
    
    /// End of a valid data frame.
    ControlByte_EndFrame                = ControlByte_StartFrame,
    
    /// Escape character. Interpret the next byte as data instead of a control
    /// byte.
    ControlByte_Escape                  = 0x55,
    
} ControlByte;


/// Defines of the different bridge commands that the host can send.
/// Due to the 0xaa framing protocol, the following command definitions are
/// prohibited:
/// 1.  ª [0xaa, 85 dec]: feminine ordinal character
/// 2.  U [0x55, 170 dec]: capital letter "U"
typedef enum BridgeCommand
{
    /// No command.
    BridgeCommand_None                  = 0x00,
    
    /// Host to bridge ACK over UART.
    BridgeCommand_Ack                   = 'A',
    
    /// Configures bridge to slave update mode. This is the old variant that is
    /// kept for backwards compatibility.
    BridgeCommand_SlaveUpdate           = 'B',
    
    /// Global error mode and error reporting.
    BridgeCommand_Error                 = 'E',
    
    /// Access the I2C slave address.
    BridgeCommand_SlaveAddress          = 'I',
    
    /// Bridge to I2C slave NAK over I2C.
    BridgeCommand_SlaveNak              = 'N',
    
    /// Bridge I2C read from I2C slave.
    BridgeCommand_SlaveRead             = 'R',
    
    /// I2C communication timeout between bridge and I2C slave.
    BridgeCommand_SlaveTimeout          = 'T',
    
    /// Bridge version information, legacy implementation.
    BridgeCommand_LegacyVersion         = 'V',
    
    /// Bridge I2C write to I2C slave.
    BridgeCommand_SlaveWrite            = 'W',
    
    /// Bridge to I2C slave ACK over I2C.
    BridgeCommand_SlaveAck              = 'a',
    
    /// Bridge reset.
    BridgeCommand_Reset                 = 'r',
    
    /// Bridge version information; updated.
    BridgeCommand_Version               = 'v',
    
} BridgeCommand;


/// Enumeration that defines the offsets of different types of bytes within the
/// UICO UART frame protocol data payload.
typedef enum PacketOffset
{
    /// Offset in the data frame for the bridge command.
    PacketOffset_BridgeCommand          = 0u,
    
    /// Offset in the data frame for the data payload.
    PacketOffset_BridgeData             = 1u,
    
    /// Offset in the data frame for the I2C address in I2C transactions (read,
    /// write, ACK).
    PacketOffset_I2cAddress             = 1u,
    
    /// Offset in the data frame for the I2C data in I2C transactions (read,
    /// write).
    PacketOffset_I2cData                = 2u,
    
} PacketOffset;


/// Enumeration that defines the offsets of the different slave update settings
/// in the data payload of the Bridgecommand_SlaveUpdate command.
typedef enum UpdateOffset
{
    /// Flags associated with the update packet. See the UpdateFlags union.
    UpdateOffset_Flags                  = 0u,
    
    /// Offset for the file size. Note this is a big-endian 16-bit value.
    UpdateOffset_FileSize               = 1u,
    
    /// Offset for the chunk size.
    UpdateOffset_ChunkSize              = 3u,
    
    /// Offset for total number of chunks.
    UpdateOffset_NumberOfChunks         = 4u,
    
    /// Offset for the delay in milliseconds (currently not used).
    UpdateOffset_DelayMS                = 5u,
    
} UpdateOffset;


/// Enumeration that defines the offsets of the different fields in the update
/// packet.
typedef enum UpdateChunkOffset
{
    /// Offset for the chunk size. Note this is a big-endian 16-bit value.
    UpdateChunkOffset_Size              = 0u,
    
    /// Offset for the data payload in the chunk.
    UpdateChunkOffset_Data              = 2u,
    
} UpdateChunkOffset;


/// Status/result of processing received data byte from the update packet.
typedef enum RxUpdateByteStatus
{
    /// Byte received fine. Continue receiving bytes for the subchunk.
    RxUpdateByteStatus_Success,
    
    /// Byte received fine. Subchunk complete, start a new subchunk.
    RxUpdateByteStatus_SubchunkComplete,
    
    /// Byte received fine. Chunk complete, start a new chunk.
    RxUpdateByteStatus_ChunkComplete,
    
    /// Byte received fine. File complete and all update data has been received,
    /// close the update.
    RxUpdateByteStatus_FileComplete,
    
    /// There was something wrong with the received byte; abort the update.
    RxUpdateByteStatus_Error,
    
} RxUpdateByteStatus;


/// Type flags that describe the type of packet.
typedef struct Flags
{
    /// Flag indicating if the packet contains a command.
    bool command : 1;
    
    /// Flag indicating if the packet contains data.
    bool data : 1;
    
} Flags;


/// Union that represents a bit-mask of different flags associated with the
/// update.
typedef union UpdateFlags
{
    /// 8-bit value representation of the bit-mask.
    uint8_t value;
    
    struct
    {
        /// Bi-directional. Purpose unknown.
        bool initiate : 1;
        
        /// Only sent by the bridge. Indicates that the update was successful.
        bool success : 1;
        
        /// Not used.
        bool writeSuccess : 1;
        
        /// Only sent by the bridge. Indicates the bridge is ready for the next
        /// update chunk.
        bool readyForNextChunk : 1;
        
        /// Only sent by the host. Indicates that the update packet contains
        /// information regarding the update file.
        bool updateFileInfo : 1;
        
        /// Only sent by the bridge. Purpose unknown.
        bool test : 1;
        
        /// Only sent by the host. Associated with the *.txt file update.
        /// Untested and behavior is unknown.
        bool textStream : 1;
        
        /// Only sent by the bridge. Indicates there was a problem with the
        /// update.
        bool error : 1;
        
    };
    
} UpdateFlags;


/// Container of variables pertaining to the current the slave update chunk.
typedef struct UpdateChunk
{
    /// The total size in bytes of the chunk, update data only, in bytes. Used
    /// to help determine when the chunk has been completely sent.
    uint16_t totalSize;
    
    /// The current size of the chunk, update data only, in bytes.
    uint16_t size;
    
    /// The current size of the subchunk, update data only, in bytes.
    uint16_t subchunkSize;
    
} UpdateChunk;


/// Settings pertaining to the slave update. Note that these parameters are
/// determined at runtime via the BridgeCommand_SlaveUpdate bridge command.
/// File:       The entire data contents of the slave firmware update.
/// Chunk:      Individual piece of the file that the host sends over the host
///             UART communication interface.
/// Subchunk:   Individual piece of the chunk that the bridge sends over the
///             lsave I2C communication interface.
///
/// F: file
/// C: chunk
/// S: subchunk
///
/// FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
/// CCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCC
/// SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS
typedef struct UpdateFile
{
    /// Pointer to the current update chunk, containing info on the current
    /// chunk int he update process; if NULL, then the module is not in
    /// update mode.
    UpdateChunk* updateChunk;
    
    /// The total size of the update file (raw data only) in bytes (unused).
    uint16_t totalSize;
    
    /// The current size of the update file (raw data only) in bytes.
    uint16_t size;
    
    /// The size of a subchunk in bytes. The subchunk includes any header info
    /// along with actual update data.
    uint16_t subchunkSize;
    
    /// The toal number of chunks to expect in the entire update.
    uint8_t totalChunks;
    
    /// The current number of chunks in the entire update.
    uint8_t chunk;
    
    /// The delay in milliseconds (unused).
    uint8_t delayMS;
    
} UpdateFile;


/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap
{
    /// Decoded receive queue.
    volatile Queue decodedRxQueue;
    
    /// Transmit queue.
    Queue txQueue;
    
    /// Array of decoded receive queue elements for the receive queue; these
    /// elements have been received but are pending processing.
    QueueElement decodedRxQueueElements[RX_QUEUE_MAX_SIZE];
    
    /// Array of transmit queue elements for the transmit queue.
    QueueElement txQueueElements[TX_QUEUE_MAX_SIZE];
    
    /// The last time data was received in milliseconds.
    volatile uint32_t lastRxTimeMS;
    
    /// The current state in the protocol state machine for receive processing.
    /// frame.
    volatile RxState rxState;
    
    /// The type flags of the data that is waiting to be enqueued into the
    /// transmit queue. This must be set prior to enqueueing data into the
    /// transmit queue.
    Flags pendingTxEnqueueFlags;
    
    /// The command associated with the data that is watiting to be enqueued
    /// into the transmit queue. This must be set prior to enqueueing data into
    /// the transmit queue.
    BridgeCommand pendingTxEnqueueCommand;
    
} Heap;


/// Data extension for the Heap structure. Defines the data buffers when in the
/// normal mode.
typedef struct HeapData
{
    /// Array to hold the decoded data of elements in the receive queue.
    uint8_t decodedRxQueueData[RX_QUEUE_DATA_SIZE];
    
    /// Array to hold the data of the elements in the transmit queue.
    uint8_t txQueueData[TX_QUEUE_DATA_SIZE];
    
} HeapData;


/// Data extension for the Heap structure. Defines the data buffers when in
/// update mode.
typedef struct UpdateData
{
    /// Current status of the update chunk.
    UpdateChunk updateChunk;
    
    /// Array to hold the decoded data of elements in the receive queue.
    uint8_t decodedRxQueueData[UPDATER_RX_QUEUE_DATA_SIZE];
    
    /// Array to hold the data of the elements in the transmit queue.
    uint8_t txQueueData[UPDATER_TX_QUEUE_DATA_SIZE];
    
} UpdateData;



/// Structure used to define the memory allocation of the heap + associated
/// heap data in normal mode. Only used to determine the organization of the
/// two data structures in unallocated memory to ensure alignment.
typedef struct NormalHeap
{
    /// Heap data structure.
    Heap heap;
    
    /// HeapData data structure when in normal mode.
    HeapData heapData;
    
} NormalHeap;


/// Structure used to define the memory allocation of the heap + associated
/// heap data in update mode. Only used to determine the organization of the
/// two data structures in unallocated memory to ensure alignment.
typedef struct UpdateHeap
{
    /// Heap data structure.
    Heap heap;
    
    /// HeapData data structure when in update mode.
    UpdateData heapData;
    
} UpdateHeap;


// === CONSTANTS ===============================================================

/// Size (in bytes) for scratch buffers.
static uint8_t const ScratchSize = 16u;

/// The amount of time between receipts of bytes before we automatically reset
/// the receive state machine.
static uint16_t const RxResetTimeoutMS = 2000u;


// === GLOBALS =================================================================

/// Heap-like memory that points to the global variables used by the module that
/// was dynamically allocated. If NULL, then the module's global variables
/// have not been dynamically allocated and the module has not started.
static Heap* g_heap = NULL;

/// Settings pertaining to the update file.
UpdateFile g_updateFile = { NULL, 0, 0, 0, 0, 0, 0 };

/// Callback function that is invoked when data is received out of the frame
/// state machine.
static UartRxOutOfFrameCallback g_rxOutOfFrameCallback = NULL;

/// Callback function that is invoked when data is received but the receive
/// buffer is not large enough to store it so the data overflowed.
static UartRxFrameOverflowCallback g_rxFrameOverflowCallback = NULL;


// === PRIVATE FUNCTIONS =======================================================

/// Checks if update mode is enabled.
/// @return If update mode is enabled.
static bool isUpdateEnabled(void)
{
    return (g_updateFile.updateChunk != NULL);
}


/// Resets the update file to default values when a new update file packet is
/// received. Note that the pointer to the UpdateChunk is not reset.
static void resetUpdateFile(void)
{
    g_updateFile.totalSize = 0;
    g_updateFile.subchunkSize = 0;
    g_updateFile.totalChunks = 0;
    g_updateFile.delayMS = 0;
    g_updateFile.size = 0;
    g_updateFile.chunk = 0;
}


/// Resets the update chunk to default values when a new update chunk is
/// received.
static void resetUpdateChunk(void)
{
    if (isUpdateEnabled())
    {
        g_updateFile.updateChunk->totalSize = 0;
        g_updateFile.updateChunk->size = 0;
        g_updateFile.updateChunk->subchunkSize = 0;
    }
}


/// Sets all the global variables pertaining to the decoded receive buffer to
/// an initial state.
static void resetRxTime(void)
{
    g_heap->lastRxTimeMS = hwSystemTime_getCurrentMS();
}


/// Resets the variables associated with the pending transmit enqueue.
static void resetPendingTxEnqueue(void)
{
    g_heap->pendingTxEnqueueCommand = BridgeCommand_None;
    static Flags const DefaultFlags = { false, false };
    g_heap->pendingTxEnqueueFlags = DefaultFlags;
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
static uint16_t encodeData(uint8_t target[], uint16_t targetSize, uint8_t const source[], uint16_t sourceSize)
{
    uint16_t t = 0;
    if ((targetSize > 0) && (target != NULL))
    {
        // Always put the start frame control byte in the beginning.
        target[t++] = ControlByte_StartFrame;
        
        bool processPendingData = true;
        if (g_heap->pendingTxEnqueueFlags.command && (g_heap->pendingTxEnqueueCommand != BridgeCommand_None))
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
                target[t++] = g_heap->pendingTxEnqueueCommand;
            }
        }
        
        if (g_heap->pendingTxEnqueueFlags.data && processPendingData && (sourceSize > 0) && (source != NULL))
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
static uint16_t __attribute__((unused)) encodeTxData(uint8_t target[], uint16_t targetSize, BridgeCommand command, Flags flags, uint8_t const source[], uint16_t sourceSize)
{
    g_heap->pendingTxEnqueueCommand = command;
    g_heap->pendingTxEnqueueFlags = flags;
    return encodeData(target, targetSize, source, sourceSize);
}


/// Enqueue a command response and any associated data into the transmit queue.
/// @param[in]  command The command associated with the transmit packet.
/// @param[in]  data    The data to enqueue. If this is NULL, then the data flag
///                     will not be set.
/// @param[in]  size    The size of the data. If the value is 0, then the data
///                     flag will not be set.
/// @return If the command responseand associated data was successfully
///         enqueued.
static bool txEnqueueCommandResponse(BridgeCommand command, uint8_t const data[], uint16_t size)
{
    bool status = false;
    if (!queue_isFull(&g_heap->txQueue) && (command != BridgeCommand_None))
    {
        bool isEmptyData = ((data == NULL) || (size <= 0));
        g_heap->pendingTxEnqueueCommand = command;
        g_heap->pendingTxEnqueueFlags.command = true;
        g_heap->pendingTxEnqueueFlags.data = !isEmptyData;
        if (isEmptyData)
        {
            // The dummyData variables serves as a placeholder to allow for a
            // successful enqueue of only commands in the txQueue. The data doesn't
            // matter. This will allow the enqueue callback function, encodeData(),
            // to populate the enqueue with the command.
            uint8_t dummyData = 0;
            queue_enqueue(&g_heap->txQueue, &dummyData, sizeof(dummyData));
        }
        else
            queue_enqueue(&g_heap->txQueue, data, size);
        status = true;
    }
    return status;
}


/// Enqueue the legacy implementation of the version request command.
/// @return If the legacy version response was successfully enqueued.
static bool txEnqueueLegacyVersion(void)
{
    uint32_t const UartBaud = 1000000u;
    static uint8_t const Version[] =
    {
        (uint8_t)VERSION_MAJOR,
        (uint8_t)VERSION_MINOR,
        (uint8_t)((UartBaud >> 24) & 0xff),
        (uint8_t)((UartBaud >> 16) & 0xff),
        (uint8_t)((UartBaud >>  8) & 0xff),
        (uint8_t)((UartBaud >>  0) & 0xff),
    };
    
    bool status = false;
    if (!queue_isFull(&g_heap->txQueue))
    {
        g_heap->pendingTxEnqueueCommand = BridgeCommand_LegacyVersion;
        g_heap->pendingTxEnqueueFlags.command = true;
        g_heap->pendingTxEnqueueFlags.data = true;
        queue_enqueue(&g_heap->txQueue, Version, sizeof(Version));
        status = true;
    }
    return status;
}


/// Enqueue the current implementation of the version request command.
/// @return If the version response was successfully enqueued.
static bool txEnqueueVersion(void)
{
    //static char const* Format = "v%u.%u [FW%03u%03u%02u%02X]";
    static uint8_t const Version[] =
    {
        HI_BYTE_16_BIT(VERSION_MAJOR),
        LO_BYTE_16_BIT(VERSION_MAJOR),
        HI_BYTE_16_BIT(VERSION_MINOR),
        LO_BYTE_16_BIT(VERSION_MINOR),
    };
    
    bool status = false;
    if (!queue_isFull(&g_heap->txQueue))
    {
        g_heap->pendingTxEnqueueCommand = BridgeCommand_Version;
        g_heap->pendingTxEnqueueFlags.command = true;
        g_heap->pendingTxEnqueueFlags.data = true;
        queue_enqueue(&g_heap->txQueue, Version, sizeof(Version));
        status = true;
    }
    return status;
}


/// Enqueue the UART-specific error response.
/// @return If the error response was successfully enqueued.
static bool __attribute__((unused)) txEnqueueUartError(uint16_t callsite)
{
    bool result = false;
    if (!queue_isFull(&g_heap->txQueue))
    {
        uint8_t scratch[ScratchSize];
        int size = error_makeUartErrorMessage(scratch, sizeof(scratch), 0, callsite);
        if (size > 0)
        {
            g_heap->pendingTxEnqueueCommand = BridgeCommand_Error;
            g_heap->pendingTxEnqueueFlags.command = true;
            g_heap->pendingTxEnqueueFlags.data = true;
            queue_enqueue(&g_heap->txQueue, scratch, sizeof(size));
            result = true;
        }
    }
    return result;
}


/// Enqueue the I2C-specific error response.
/// @param[in]  status
/// @param[in]  callsite    Unique callsite ID to distinguish different
///                         functions that triggered the error.
/// @return If the error response was successfully enqueued.
static bool txEnqueueI2cError(I2cTouchStatus status, uint16_t callsite)
{
    bool result = false;
    if (!queue_isFull(&g_heap->txQueue))
    {
        uint8_t scratch[ScratchSize];
        int size = error_makeI2cErrorMessage(scratch, sizeof(scratch), status, callsite);
        if (size > 0)
        {
            g_heap->pendingTxEnqueueCommand = BridgeCommand_Error;
            g_heap->pendingTxEnqueueFlags.command = true;
            g_heap->pendingTxEnqueueFlags.data = true;
            queue_enqueue(&g_heap->txQueue, scratch, size);
            result = true;
        }
    }
    return result;
}


/// Processes errors from the I2C gen 2 module, specifically prep an error
/// message to send to the host.
/// @param[in]  status      Status indicating if an error occured during the I2c
///                         transaction. See the definition of the I2cTouchStatus
///                         union.
/// @param[in]  callsite    Unique callsite ID to distinguish different
///                         functions that triggered the error.
static void processI2cErrors(I2cTouchStatus status, uint16_t callsite)
{
    
    if (error_getMode() == ErrorMode_Global)
        txEnqueueI2cError(status, callsite);
    else
    {
        if (status.deactivated)
            ;
        if (status.driverError)
            ;
        if (status.timedOut)
            txEnqueueCommandResponse(BridgeCommand_SlaveTimeout, NULL, 0);
        if (status.nak)
            txEnqueueCommandResponse(BridgeCommand_SlaveNak, NULL, 0);
        if (status.invalidRead)
            ;
        if (status.queueFull)
            ;
        if (status.invalidInputParameters)
            ;
    }
    error_tally(ErrorType_I2c);
}


/// Processes the error command from the host and enqueues the appropriate
/// response.
/// @param[in]  data    The data payload from the error command.
/// @param[in]  size    The size of the data payload.
/// @return If the appropriate response was successfully enqueued.
static bool processErrorCommand(uint8_t const* data, uint16_t size)
{
    if ((data != NULL) && (size > 0))
        error_setMode((data[0] != 0) ? (ErrorMode_Global) : (ErrorMode_Legacy));
    
    bool status = false;
    if (!queue_isFull(&g_heap->txQueue))
    {
        uint8_t scratch[ScratchSize];
        int size = error_makeModeMessage(scratch, sizeof(scratch));
        if (size > 0)
        {
            g_heap->pendingTxEnqueueCommand = BridgeCommand_Error;
            g_heap->pendingTxEnqueueFlags.command = true;
            g_heap->pendingTxEnqueueFlags.data = true;
            queue_enqueue(&g_heap->txQueue, scratch, size);
            status = true;
        }
    }
    return status;
}


/// Processes the slave update command from the host.
/// @param[in]  data    The data payload from the error command.
/// @param[in]  size    The size of the data payload.
/// @return If the slave update command was processed successfully.
static bool processSlaveUpdateCommand(uint8_t const* data, uint16_t size)
{
    static uint16_t const MinChunkDataSize = 8u;
    static uint16_t const MinChunkHeaderSize = 14u;
    static uint16_t const MinChunkSize = MinChunkDataSize + MinChunkHeaderSize;
    static uint16_t const ChunkSizeAdjustment = 256u;
    
    bool status = false;
    if (size > UpdateOffset_Flags)
    {
        UpdateFlags flags = { data[UpdateOffset_Flags] };
        if (flags.initiate)
        {
            // @TODO: figure out what this flag is supposed to do.
        }
        if (flags.updateFileInfo)
        {
            if (size > UpdateOffset_DelayMS)
            {
                g_updateFile.size = utility_bigEndianUint16(&data[UpdateOffset_FileSize]);
                g_updateFile.subchunkSize = data[UpdateOffset_ChunkSize];
                if (g_updateFile.subchunkSize < MinChunkSize)
                    g_updateFile.subchunkSize += ChunkSizeAdjustment;
                g_updateFile.totalChunks = data[UpdateOffset_NumberOfChunks];
                g_updateFile.delayMS = data[UpdateOffset_ChunkSize];
            }
            status = true;
        }
        if (flags.textStream)
        {
            // @TODO: figure out what this flag is supposed to do.
        }
    }
    if (!status)
    {
        // @TODO: send an update error message indicating the flag wasn't
        // recognized.
    }
    return status;
}


/// Processes the decoded UART receive packet (where the frame and escape
/// characters are removed).
/// @param[in]  data    The decoded received packet.
/// @param[in]  size    The number of bytes in the decoded received packet.
/// @return If the UART command was successfully processed or not.
static bool processDecodedRxPacket(uint8_t* data, uint16_t size)
{
    bool status = false;
    if ((data != NULL) && (size > PacketOffset_BridgeCommand))
    {
        status = true;
        uint8_t command = data[PacketOffset_BridgeCommand];
        switch (command)
        {
            case BridgeCommand_Ack:
            {
                txEnqueueCommandResponse(BridgeCommand_Ack, NULL, 0);
                break;
            }
            
            case BridgeCommand_Error:
            {
                processErrorCommand(&data[PacketOffset_BridgeData], size - PacketOffset_BridgeData);
                break;
            }
            
            case BridgeCommand_SlaveAddress:
            {
                if (size > PacketOffset_I2cAddress)
                    i2cTouch_setSlaveAddress(data[PacketOffset_I2cAddress]);
                else
                    status = false;
                break;
            }
            
            case BridgeCommand_SlaveRead:
            {
                if (size > PacketOffset_I2cData)
                    i2cTouch_read(data[PacketOffset_I2cAddress], data[PacketOffset_I2cData]);
                else if (size > PacketOffset_I2cAddress)
                    // Read at least one byte.
                    i2cTouch_read(data[PacketOffset_I2cAddress], 1u);
                break;
            }
            
            case BridgeCommand_LegacyVersion:
            {
                txEnqueueLegacyVersion();
                break;
            }
            
            case BridgeCommand_SlaveWrite:
            {
                if (size > PacketOffset_I2cData)
                    i2cTouch_write(data[PacketOffset_I2cAddress], &data[PacketOffset_I2cData], size - PacketOffset_I2cData);
                break;
            }
            
            case BridgeCommand_SlaveAck:
            {
                I2cTouchStatus i2cStatus;
                if (size > PacketOffset_BridgeData)
                    i2cStatus = i2cTouch_ack(data[PacketOffset_BridgeData], 0);
                else
                    i2cStatus = i2cTouch_ackApp(0);
                if (!i2cStatus.errorOccurred)
                    txEnqueueCommandResponse(BridgeCommand_SlaveAck, NULL, 0);
                break;
            }
            
            case BridgeCommand_SlaveUpdate:
            {
                if (size > PacketOffset_BridgeData)
                    processSlaveUpdateCommand(&data[PacketOffset_BridgeData], size - PacketOffset_BridgeData);
                break;
            }
            
            case BridgeCommand_Reset:
            {
                CySoftwareReset();
                break;
            }
            
            case BridgeCommand_Version:
            {
                txEnqueueVersion();
                break;
            }
            
            default:
            {
                // Should not get here.
                status = false;
                break;
            }
        }
    }
    return status;
}


/// Processes the received data payload byte from the update packet. These bytes
/// already have the 0xaa framing and header information parsed out.
/// @param[in]  data    The byte to process.
/// @return The RxUpdatebyteStatus indicating success, error, or if the byte
///         corresponds to the end of a subchunk, chunk or update file.
static RxUpdateByteStatus processRxUpdateByte(uint8_t data)
{
    RxUpdateByteStatus status = RxUpdateByteStatus_Success;
    if (queue_enqueueByte(&g_heap->decodedRxQueue, data, false))
    {
        g_updateFile.updateChunk->subchunkSize++;
        g_updateFile.updateChunk->size++;
        g_updateFile.size++;
        if (g_updateFile.size >= g_updateFile.totalSize)
        {
            
            queue_enqueueFinalize(&g_heap->decodedRxQueue);
            status = RxUpdateByteStatus_FileComplete;
        }
        else if (g_updateFile.updateChunk->size >= g_updateFile.updateChunk->totalSize)
        {
            queue_enqueueFinalize(&g_heap->decodedRxQueue);
            status = RxUpdateByteStatus_ChunkComplete;
        }
        else if (g_updateFile.updateChunk->subchunkSize >= g_updateFile.subchunkSize)
        {
            queue_enqueueFinalize(&g_heap->decodedRxQueue);
            g_updateFile.updateChunk->subchunkSize = 0;
            status = RxUpdateByteStatus_SubchunkComplete;
        }
    }
    else
        status = RxUpdateByteStatus_Error;
    return status;
}


/// Processes the received byte and removes the framing protocol to get a pure
/// data buffer.
/// @param[in]  data    The byte to process.
/// @return If the byte was valid data when processing through the framing
///         protocol.
static bool processRxByte(uint8_t data)
{
    bool status = true;
    switch (g_heap->rxState)
    {
        case RxState_OutOfFrame:
        {
            if (data == ControlByte_StartFrame)
            {                        
                resetRxTime();
                if (isUpdateEnabled())
                    g_heap->rxState = RxState_UpdatePacketSizeHiByte;
                else
                    g_heap->rxState = RxState_InFrame;
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
                g_heap->rxState = RxState_EscapeCharacter;
            else if (isEndFrameCharacter(data))
            {
                status = queue_enqueueFinalize(&g_heap->decodedRxQueue);
                g_heap->rxState = RxState_OutOfFrame;
            }
            else
            {
                status = queue_enqueueByte(&g_heap->decodedRxQueue, data, false);
                if (!status)
                    handleRxFrameOverflow(data);
            }
            break;
        }
        
        case RxState_EscapeCharacter:
        {
            status = queue_enqueueByte(&g_heap->decodedRxQueue, data, false);
            if (!status)
                handleRxFrameOverflow(data);
            g_heap->rxState = RxState_InFrame;
            break;
        }
        
        case RxState_UpdatePacketSizeHiByte:
        {
            resetUpdateChunk();
            g_updateFile.updateChunk->totalSize = (uint16_t)data << 8u;
            g_heap->rxState = RxState_UpdatePacketSizeLoByte;
            break;
        }
        
        case RxState_UpdatePacketSizeLoByte:
        {
            g_updateFile.updateChunk->totalSize += (uint16_t)data;
            g_heap->rxState = RxState_UpdatePacketData;
            break;
        }
        
        case RxState_UpdatePacketData:
        {
            RxUpdateByteStatus updateStatus = processRxUpdateByte(data);
            switch (updateStatus)
            {
                case RxUpdateByteStatus_Success:
                {
                    break;
                }
                
                case RxUpdateByteStatus_SubchunkComplete:
                {
                    break;
                }
                
                case RxUpdateByteStatus_ChunkComplete:
                {
                    break;
                }
                
                case RxUpdateByteStatus_FileComplete:
                {
                    break;
                }
                
                case RxUpdateByteStatus_Error:
                {
                    // @TODO: do some error handling.
                    break;
                }
                
                default:
                {
                    // @TODO: do some error handling.
                }
            }
            break;
        }
        
        default:
        {
            // We should never get into this state, if we do, something
            // wrong happened.  Potentially do some error handling.
            
            // Reset to the default state: out of frame.
            g_heap->rxState = RxState_OutOfFrame;
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
static uint16_t __attribute__((unused)) processReceivedData(uint8_t const source[], uint16_t sourceSize, uint16_t sourceOffset)
{
    // Track the number of bytes that was processed.
    uint32_t size = 0;
    
    if (g_heap != NULL)
    {
        // Iterate through all the received bytes via UART.
        for (uint32_t i = sourceOffset; i < sourceSize; ++i)
        {
            uint8_t data = source[i];
            if (processRxByte(data))
                ++size;
        }
    }
    return size;
}


/// Initializes the basic receive variables
static void initRx(void)
{
    g_heap->rxState = RxState_OutOfFrame;
    resetRxTime();
}


/// Initializes the decoded receive queue when in standard/normal mode.
/// @param[in]  heap    Pointer to the specific normal heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initDecodedRxQueue(NormalHeap* heap)
{
    queue_deregisterEnqueueCallback(&g_heap->decodedRxQueue);
    g_heap->decodedRxQueue.data = heap->heapData.decodedRxQueueData;
    g_heap->decodedRxQueue.elements = g_heap->decodedRxQueueElements;
    g_heap->decodedRxQueue.maxDataSize = RX_QUEUE_DATA_SIZE;
    g_heap->decodedRxQueue.maxSize = RX_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->decodedRxQueue);
    resetRxTime();
}


/// Initializes the transmit queue when in standard/normal mode.
/// @param[in]  heap    Pointer to the specific normal heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initTxQueue(NormalHeap* heap)
{
    queue_registerEnqueueCallback(&g_heap->txQueue, encodeData);
    g_heap->txQueue.data = heap->heapData.txQueueData;
    g_heap->txQueue.elements = g_heap->txQueueElements;
    g_heap->txQueue.maxDataSize = TX_QUEUE_DATA_SIZE;
    g_heap->txQueue.maxSize = TX_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->txQueue);
    resetPendingTxEnqueue();
}


/// Initializes the decoded receive queue when in update mode.
/// @param[in]  heap    Pointer to the specific update heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initUpdateDecodedRxQueue(UpdateHeap* heap)
{
    queue_deregisterEnqueueCallback(&g_heap->decodedRxQueue);
    g_heap->decodedRxQueue.data = heap->heapData.decodedRxQueueData;
    g_heap->decodedRxQueue.elements = g_heap->decodedRxQueueElements;
    g_heap->decodedRxQueue.maxDataSize = UPDATER_RX_QUEUE_DATA_SIZE;
    g_heap->decodedRxQueue.maxSize = RX_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->decodedRxQueue);
    resetRxTime();
}


/// Initializes the transmit queue when in update mode.
/// @param[in]  heap    Pointer to the specific update heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initUpdateTxQueue(UpdateHeap* heap)
{
    queue_registerEnqueueCallback(&g_heap->txQueue, encodeData);
    g_heap->txQueue.data = heap->heapData.txQueueData;
    g_heap->txQueue.elements = g_heap->txQueueElements;
    g_heap->txQueue.maxDataSize = UPDATER_TX_QUEUE_DATA_SIZE;
    g_heap->txQueue.maxSize = TX_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->txQueue);
    resetPendingTxEnqueue();
}


/// Initializes the update packet when in update mode.
/// @param[in]  heap    Pointer to the specific update heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initUpdatePacket(UpdateHeap* heap)
{
    g_updateFile.updateChunk = &heap->heapData.updateChunk;
}


/// Register the callback functions for I2C-related events.
static void registerI2cCallbacks(void)
{
    i2cTouch_registerRxCallback(uartCommon_txEnqueueData);
    i2cTouch_registerErrorCallback(processI2cErrors);
}


// === ISR =====================================================================

/// ISR for UART IRQ's in general.
static void isr(void)
{
    uint32_t source = COMPONENT(HOST_UART, GetRxInterruptSource)();
    if ((source & COMPONENT(HOST_UART, INTR_RX_NOT_EMPTY)) != 0)
    {
        uint32_t data = COMPONENT(HOST_UART, UartGetByte)();
        if (data > 0xff)
        {
            // @TODO: Error handling.
        }
        else if (g_heap != NULL)
            processRxByte(data);
        COMPONENT(HOST_UART, ClearRxInterruptSource)(COMPONENT(HOST_UART, INTR_RX_NOT_EMPTY));
    }
    else if ((source & COMPONENT(HOST_UART, INTR_RX_FRAME_ERROR)) != 0)
    {
        // Do some special handling for a frame error; possibly determine baud
        // rate.
        COMPONENT(HOST_UART, ClearRxInterruptSource)(COMPONENT(HOST_UART, INTR_RX_FRAME_ERROR));
    }
    COMPONENT(HOST_UART, ClearPendingInt)();
}


// === PUBLIC FUNCTIONS ========================================================

void uartCommon_init(void)
{
    uartCommon_deactivate();
    
    // Setup the UART hardware.
    COMPONENT(HOST_UART, SetCustomInterruptHandler)(isr);
    COMPONENT(HOST_UART, Start)();
}


uint16_t uartCommon_getHeapWordRequirement(bool enableUpdate)
{
    uint16_t size = (enableUpdate) ? (sizeof(UpdateHeap)) : (sizeof(NormalHeap));
    return heap_calculateHeapWordRequirement(size);
}


uint16_t uartCommon_activate(heapWord_t* memory, uint16_t size, bool enableUpdate)
{
    uint16_t allocatedSize = 0;
    uint16_t requiredSize = uartCommon_getHeapWordRequirement(enableUpdate);
    if ((memory != NULL) && (size >= requiredSize))
    {
        g_heap = (Heap*)memory;
        if (enableUpdate)
        {
            UpdateHeap* heap = (UpdateHeap*)g_heap;
            initUpdateDecodedRxQueue(heap);
            initUpdateTxQueue(heap);
            initUpdatePacket(heap);
            resetUpdateFile();
        }
        else
        {
            NormalHeap* heap = (NormalHeap*)g_heap;
            initDecodedRxQueue(heap);
            initTxQueue(heap);
            g_updateFile.updateChunk = NULL;
        }
        initRx();
        registerI2cCallbacks();
        allocatedSize = requiredSize;
    }
    return allocatedSize;
}


uint16_t uartCommon_deactivate(void)
{
    uint16_t size = 0u;
    if (g_heap != NULL)
    {
        size = uartCommon_getHeapWordRequirement(isUpdateEnabled());
        g_heap = NULL;
    }
    resetUpdateFile();
    g_updateFile.updateChunk = NULL;
    return size;
}


bool uartCommon_isActivated(void)
{
    return ((g_heap != NULL) && !isUpdateEnabled());
}


bool uartCommon_isUpdateActivated(void)
{
    return ((g_heap != NULL) && isUpdateEnabled());
}


void uartCommon_registerRxOutOfFrameCallback(UartRxOutOfFrameCallback callback)
{
    if (callback != NULL)
        g_rxOutOfFrameCallback = callback;
}


void uartCommon_registerRxFrameOverflowCallback(UartRxFrameOverflowCallback callback)
{
    if (callback != NULL)
        g_rxFrameOverflowCallback = callback;
}


bool uartCommon_isTxQueueEmpty(void)
{
    bool empty = false;
    if (g_heap != NULL)
        empty = queue_isEmpty(&g_heap->txQueue);
    return empty;
}


uint16_t uartCommon_processRx(uint32_t timeoutMS)
{
    uint16_t count = 0;
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_ContinuousNotification);
        else
            alarm_disarm(&alarm);
            
        while (!queue_isEmpty(&g_heap->decodedRxQueue))
        {
            if (alarm.armed && alarm_hasElapsed(&alarm))
                break;
            
            uint8_t* data;
            uint16_t size = queue_dequeue(&g_heap->decodedRxQueue, &data);
            if (size > 0)
                if (processDecodedRxPacket(data, size))
                    ++count;
        }
    }
    return count;
}


uint16_t uartCommon_processTx(uint32_t timeoutMS)
{
    uint16_t count = 0;
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS > 0)
            alarm_arm(&alarm, timeoutMS, AlarmType_ContinuousNotification);
        else
            alarm_disarm(&alarm);
            
        while (!queue_isEmpty(&g_heap->txQueue))
        {
            if (alarm.armed && alarm_hasElapsed(&alarm))
                break;
            
            uint8_t* data;
            uint16_t size = queue_dequeue(&g_heap->txQueue, &data);
            if (size > 0)
            {
                for (uint32_t i = 0; i < size; ++i)
                    COMPONENT(HOST_UART, UartPutChar)(data[i]);
                ++count;
            }
        }
    }
    return count;
}


bool uartCommon_txEnqueueData(uint8_t const data[], uint16_t size)
{
    bool status = false;
    if ((g_heap != NULL) && !queue_isFull(&g_heap->txQueue))
    {
        g_heap->pendingTxEnqueueCommand = BridgeCommand_None;
        g_heap->pendingTxEnqueueFlags.command = false;
        g_heap->pendingTxEnqueueFlags.data = true;
        status = queue_enqueue(&g_heap->txQueue, data, size); 
    }
    return status;
}


bool uartCommon_txEnqueueError(uint8_t const data[], uint16_t size)
{
    bool status = false;
    if (error_getMode() == ErrorMode_Global)
    {
        if ((g_heap != NULL) && !queue_isFull(&g_heap->txQueue))
        {
            g_heap->pendingTxEnqueueCommand = BridgeCommand_Error;
            g_heap->pendingTxEnqueueFlags.command = true;
            g_heap->pendingTxEnqueueFlags.data = true;
            status = queue_enqueue(&g_heap->txQueue, data, size); 
        }
    }
    return status;
}


void uartCommon_write(char const string[])
{
    if (g_heap == NULL)
        COMPONENT(HOST_UART, UartPutString)(string);
}


/* [] END OF FILE */