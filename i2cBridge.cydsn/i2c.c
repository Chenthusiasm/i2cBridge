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

#include "i2c.h"

#include <stdio.h>

#include "alarm.h"
#include "debug.h"
#include "error.h"
#include "i2cTouch.h"
#include "i2cUpdate.h"
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

/// Enable/disable checking if on slave IRQ, if a write to change to the slave
/// app's response buffer must be done before reading.
/// true:   always change to the response buffer on every interrupt.
/// false:  check the contents of the read to determine if the switch to
///         response buffer must always be done on interrupt.
#define ENABLE_ALL_CHANGE_TO_RESPONSE   (false)

/// Size of the raw receive data buffer in touch mode.
#define TOUCH_RX_BUFFER_SIZE            (260u)

/// Size of the raw receive data buffer in update mode.
#define UPDATE_RX_BUFFER_SIZE           (32u)

/// The max size of the transfer queue (the max number of queue elements).
#define XFER_QUEUE_MAX_SIZE             (8u)

/// The size of the data array that holds the queue element data in the transfer
/// queue.
#define XFER_QUEUE_DATA_SIZE            (600u)


// === TYPE DEFINES ============================================================

/// The master driver level function status type. The I2CMasterStatus
/// function returns a uint32_t, but the returned value only uses 16-bits at
/// most.
typedef uint16_t mstatus_t;


/// The master driver level function return type. The I2CMasterWriteBuf,
/// I2CMasterReadBuf, and I2CMasterSendStop functions return a uint32_t.
/// Only the I2CMasterSendStop function requires the return type to be
/// uint32_t due to the timeout error. The other two functions' return type
/// can fit in a uint16_t. Therefore, the return type is defined as the
///larger uint32_t.
typedef uint32_t mreturn_t;


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


/// Definition of the host transfer queue data offsets.
typedef enum XferQueueDataOffset
{
    /// The I2C address.
    XferQueueDataOffset_Xfer            = 0u,
    
    /// The start of the data payload.
    XferQueueDataOffset_Data            = 1u,
    
} XferQueueDataOffset;


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


/// Definition of all the available commands of the application.
typedef enum AppCommand
{
    /// Default touch scan mode: scan and only report changes in touch status.
    AppCommand_ScanAndReportChanges     = 0x01,
    
    /// Scan and report everything.
    AppCommand_ScanAndReportAll         = 0x02,
    
    /// Stop scanning.
    AppCommand_StopScan                 = 0x03,
    
    /// Accessor to modify a parameter.
    AppCommand_SetParameter             = 0x04,
    
    /// Accessor to read a parameter.
    AppCommand_GetParameter             = 0x05,
    
    /// Perform a touch rebaseline.
    AppCommand_Rebaseline               = 0x06,
    
    /// Get reset info or execute a reset.
    AppCommand_Reset                    = 0x07,
    
    /// Clear the tuning settings in flash.
    AppCommand_EraseFlash               = 0x08,
    
    /// Save current tuning settings from RAM to flash.
    AppCommand_WriteFlash               = 0x09,
    
    /// Echo test.
    AppCommand_Echo                     = 0x0a,
    
    /// Built-in self-test.
    AppCommand_Bist                     = 0x0b,
    
    /// Customer-specific command.
    AppCommand_CustomerSpecific         = 0x0c,
    
    /// Debug command.
    AppCommand_Debug                    = 0x0d,
} AppCommand;


/// States used by the communication finite state machine (FSM) to handle the
/// different steps in processing read/write transactions on the I2C bus.
typedef enum CommState
{
    /// The state machine free and is waiting for the next transaction.
    CommState_Waiting,
    
    /// Data to be read from the slave is pending.
    CommState_RxPending,
    
    /// App needs to be switched to the response buffer being active.
    CommState_RxSwitchToResponseBuffer,
    
    /// Read the length of the response.
    CommState_RxReadLength,
    
    /// Process the length after reading.
    CommState_RxProcessLength,
    
    /// Read the remaining extra data payload state.
    CommState_RxReadExtraData,
    
    /// Process the the extra data payload after reading.
    CommState_RxProcessExtraData,
    
    /// Clear the IRQ after a complete read.
    CommState_RxClearIrq,
    
    /// Check if the last receive transaction has completed.
    CommState_RxCheckComplete,
    
    /// Dequeue from the transfer queue and act (read or write) based on the
    /// type of transfer.
    CommState_XferDequeueAndAct,
    
    /// Check if the last read transfer queue transaction has completed.
    CommState_XferRxCheckComplete,
    
    /// Check if the last write transfer queue transaction has completed.
    CommState_XferTxCheckComplete,
    
} CommState;


/// I2C transfer direction: read or write.
typedef enum I2cDirection
{
    /// Write to the slave device.
    I2cDirection_Write                  = COMPONENT(SLAVE_I2C, I2C_WRITE_XFER_MODE),
    
    /// Read from the slave device.
    I2cDirection_Read                   = COMPONENT(SLAVE_I2C, I2C_READ_XFER_MODE),
    
} I2cDirection;


/// Contains information about the I2C tranfer including the slave address and
/// the direction (read or write).
typedef union I2cXfer
{
    /// 8-bit representation of
    uint8_t value;
    
    /// Anonymous struct that defines the different fields in the I2cXfer union.
    struct
    {
        /// The 7-bit slave address.
        uint8_t address : 7;
        
        /// The direction (read or write) of the transfer.
        I2cDirection direction : 1;
        
    };
    
} I2cXfer;


/// Contains the results of the processRxLength function.
typedef struct AppRxLengthResult
{
    /// Anonymous union that collects bit flags indicating specific problems
    /// in the length packet.
    union
    {
        /// General flag indicating the length packet was invalid.
        bool invalid;
        
        /// Anonymous struct to consolidate the different bit flags.
        struct
        {
            /// The command is invalid.
            bool invalidCommand : 1;
            
        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
            
            /// An invalid command was read and probably caused because the app
            /// was not in the valid buffer (valid buffer = response buffer).
            bool invalidAppBuffer : 1;
            
        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
            
            /// The length is invalid.
            bool invalidLength : 1;
            
            /// The parameters passed in to process are invalid.
            bool invalidParameters : 1;
        
        };
    };
    
    /// The size of of the data payload in bytes; if 0, then there was either an
    /// error or there is no additional data to receive.
    uint8_t dataPayloadSize;
    
} AppRxLengthResult;


/// Structure to hold variables associated with the communication finite state
/// machine (FSM).
typedef struct CommFsm
{
    /// Timeout alarm used to determine if the I2C receive transaction has timed
    /// out.
    Alarm timeoutAlarm;
    
    /// Number of bytes that need to be received (pending).
    uint16_t pendingRxSize;
    
    /// The slave IRQ was triggered so the slave indicating there's data to be
    /// read. This needs to be volatile because it's modified in an ISR.
    volatile bool rxPending;
    
    /// Flag indicating if another attempt at a read should be performed but
    /// switch to the response buffer first.
    bool rxSwitchToResponseBuffer;
    
    /// The current state.
    CommState state;
    
} CommFsm;


#if ENABLE_I2C_LOCKED_BUS_DETECTION
    
    /// Locked bus variables.
    typedef struct LockedBus
    {
        /// Alarm to determine after how long since a continuous bus busy error
        /// from the low level driver should the status flag be set indicating a
        /// locked bus.
        Alarm detectAlarm;
        
        /// Alarm to track when to attempt a recovery after a locked bus has
        /// been detected.
        Alarm recoverAlarm;
        
        /// Track the number of recovery attempts since the locked bus was
        /// detected. This can be used to determine when to trigger a system
        /// reset.
        uint8_t recoveryAttempts;
        
        /// Flag indicating if the bus is in the locked condition.
        bool locked;
        
    } LockedBus;

#endif // ENABLE_I2C_LOCKED_BUS_DETECTION
    
    
/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap
{
    /// Pointer to the queue data structure used by the module..
    Queue* queue;
    
    /// Pointer to the raw receive data buffer.
    uint8_t* rxBuffer;
    
    /// Size of the raw receive data buffer.
    uint16_t rxBufferSize;
    
    /// The I2C address and direction associated with the transaction that is
    /// waiting to be enqueued into the transfer queue. This must be set prior
    /// to enqueueing data into the transfer queue.
    I2cXfer pendingQueueXfer;
    
} Heap;


/// Data extension for the Heap structure. Defines the data buffers when in the
/// touch mode.
typedef struct TouchHeapData
{
    /// Host transfer queue.
    Queue xferQueue;
    
    /// Array of transfer queue elements for the transfer queue.
    QueueElement xferQueueElements[XFER_QUEUE_MAX_SIZE];
    
    /// Array to hold the data of the elements in the transfer queue. Note that
    /// each transfer queue element has at least 2 bytes:
    /// [0]: I2cXfer (adress and direction)
    /// [1]:
    ///     read: number of bytes to read.
    ///     write: data payload...
    uint8_t xferQueueData[XFER_QUEUE_DATA_SIZE];
    
    /// The raw receive buffer.
    uint8_t rxBuffer[TOUCH_RX_BUFFER_SIZE];
    
} TouchHeapData;


/// Data extension for the Heap structure. Defines the data buffers when in
/// update mode.
typedef struct UpdateHeapData
{
    /// The raw receive buffer.
    uint8_t rxBuffer[UPDATE_RX_BUFFER_SIZE];
    
} UpdateHeapData;


/// Structure used to define the memory allocation of the heap + associated
/// heap data in touch mode. Only used to determine the organization of the
/// two data structures in unallocated memory to ensure alignment.
typedef struct TouchHeap
{
    /// Common heap data structure.
    Heap heap;
    
    /// HeapData data structure when in normal touch mode.
    TouchHeapData heapData;
    
} TouchHeap;


/// Structure used to define the memory allocation of the heap + associated
/// heap data in update mode. Only used to determine the organization of the
/// two data structures in unallocated memory to ensure alignment.
typedef struct UpdateHeap
{
    /// Common heap data structure.
    Heap heap;
    
    /// HeapData data structure when in update mode.
    UpdateHeapData heapData;
    
} UpdateHeap;


/// Data structure that defines how the callsite variable used for error
/// tracking is organized.
///
/// [0:1]:  lowLevelCall
/// [2]:    isBusReady
/// [3]:    recoverFromLockedBus
/// [4:7]:  subCall
/// [8:15]: topCall
///
/// [0:7]:  subValue
/// [8:15]: topCall
///
/// [0:15]: value;
typedef union Callsite
{
    /// 16-bit value of the callsite as defined by callsite_t in the error
    /// module.
    callsite_t value;
    
    /// Anonymous struct that collects the 2 main parts of the callsite: the
    /// main site and sub value. 
    struct
    {
        /// Anonymous union that provides an alias for the sub value's members.
        union
        {
            /// Alias for the sub value's members.
            uint8_t subValue;
            
            /// Anonymous struct that collects the sub values invidual members.
            struct
            {
                /// Value of the lowest level private function call.
                uint8_t lowLevelCall : 2;
                
                /// Flag indicating if the isBusReady function was called.
                bool isBusReady : 1;
                
                /// Flag indicating if the recoverFromLockedBus function was
                /// called.
                bool recoverFromLockedBus : 1;
                
                /// Next level call below the top-level call. Used to help
                /// identify where specifically the error occurred.
                uint8_t subCall : 4;
                
            };
        };
        
        /// The public function that serves as the top-level invocation in the
        /// call chain within the module.
        uint8_t topCall : 8;
        
    };
    
} Callsite;


// === PRIVATE GLOBAL CONSTANTS ================================================

/// The number of bytes to read in order to find the number of bytes in the
/// receive data payload.
static uint8_t const G_AppRxPacketLengthSize = AppRxPacketOffset_Length + 1u;

/// The value of the length byte which indicates the packet is invalid.
static uint8_t const G_InvalidRxAppPacketLength = 0xff;

/// The default amount of time to allow for a I2C stop condition to be sent
/// before timing out. If a time out occurs, the I2C module is reset. This is
/// in milliseconds.
static uint32_t const G_DefaultSendStopTimeoutMs = 5u;

/// Message to write to the I2C slave to clear the IRQ. This can also be used
/// to switch to the response buffer.
static uint8_t const G_ClearIrqMessage[] = { AppBufferOffset_Response, 0 };

/// Size of the clear slave IRQ message in bytes.
static uint8_t const G_ClearIrqSize = sizeof(G_ClearIrqMessage);

/// Size of the switch to response buffer message in bytes; this uses the clear
/// slave IRQ message.
static uint8_t const G_ResponseBufferSize = sizeof(G_ClearIrqMessage) - 1u;

/// Default transfer mode mask for the low-level I2C driver reads/writes.
static uint8_t const G_DefaultTransferMode = COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER);

/// Default timeout for the alarm for detecting a locked bus condition.
static uint32_t const G_DefaultLockedBusDetectTimeoutMs = 100u;

/// Default timeout for the alarm used to determine how often to attempt to
/// recover from the locked bus.
static uint32_t const G_DefaultLockedBusRecoveryPeriodMs = 50u;

/// Max number of recovery attempts before performing a system reset.
static uint8_t const G_MaxRecoveryAttempts = 10u;

/// The default I2cStatus with no error flags set.
I2cStatus const G_NoErrorI2cStatus = { 0u };


// === PRIVATE GLOBALS =========================================================

/// Heap-like memory that points to the global variables used by the module that
/// was dynamically allocated. If NULL, then the module's global variables
/// have not been dynamically allocated and the module has not started.
static Heap* g_heap = NULL;

/// The current 7-bit slave address. When the slaveIRQ line is asserted, a read
/// will be performed from this address.
static uint8_t g_slaveAddress = SlaveAddress_App;

/// App receive state machine variables.
static CommFsm g_commFsm;

#if ENABLE_I2C_LOCKED_BUS_DETECTION
    
    /// Container for locked-bus related variables.
    static LockedBus g_lockedBus;
    
#endif // ENABLE_I2C_LOCKED_BUS_DETECTION

#if !ENABLE_ALL_CHANGE_TO_RESPONSE
    
    /// Flag indicating we're in the response buffer is active for the slave app.
    static bool g_slaveAppResponseActive = false;
    
    /// Flag indicating on receive, if a write needs to be done to switch to the
    /// response buffer.
    static bool g_appRxSwitchToResponse = false;
    
#endif // !ENABLE_ALL_CHANGE_TO_RESPONSE

/// The receive callback function.
static I2cRxCallback g_rxCallback = NULL;

/// The error callback function.
static I2cErrorCallback g_errorCallback = NULL;

/// The status of the last driver API call. Refer to the possible error messages
/// in the generated "I2C Component Name"_I2C.h file. Note that the return type
/// of the function to get the status is uint32_t, but in actuality, only a
/// uint16_t is returned.
static mstatus_t g_lastDriverStatus = 0u;

/// The return value of the last driver API call. Refer to possible errors in
/// the generated "I2C Component Name"_I2C.h file.
static mreturn_t g_lastDriverReturnValue = 0u;

/// The current callsite; the callsite is a unique ID used to help identify
/// where an error may have occurred.
static Callsite g_callsite = { 0u };


// === PRIVATE FUNCTIONS =======================================================

/// Generates the transfer queue data to include the I2C address and direction
/// as the first byte (see I2cXfer union). The transfer dequeue function
/// will take care of properly pulling out the I2C address, direction, and the
/// actual data payload to for the transaction. Note that g_pendingQueueXfer
/// must be set properly before invoking this function.
/// @param[in]  source      The source buffer.
/// @param[in]  sourceSize  The number of bytes in the source.
/// @param[out] target      The target buffer (where the formatted data is
///                         stored).
/// @param[in]  targetSize  The number of bytes available in the target.
/// @return The number of bytes in the target buffer or the number of bytes
///         to transmit.  If 0, then the source buffer was either invalid or
///         there's not enough bytes in target buffer to store the formatted
///         data.
static uint16_t prepareXferQueueData(uint8_t target[], uint16_t targetSize, uint8_t const source[], uint16_t sourceSize)
{
    static uint16_t const MinSourceSize = XferQueueDataOffset_Xfer + 1u;
    
    uint16_t size = 0;
    if ((source != NULL) && (sourceSize >= MinSourceSize) && (target != NULL) && (targetSize > sourceSize))
    {
        target[size++] = g_heap->pendingQueueXfer.value;
        memcpy(&target[size], source, sourceSize);
        size += sourceSize;
    }
    return size;
}


/// Resets the variables associated with the pending transmit enqueue.
void resetPendingTxEnqueue(void)
{
    g_heap->pendingQueueXfer.value = 0;
}


/// Checks if the app needs to switch to the response buffer when an IRQ occurs
/// indicating data is ready to be read (receive).
/// @return If the app needs to switch to the response buffer before performing
///         a read.
static bool switchToAppResponseBuffer(void)
{
    bool result = true;
    
#if !ENABLE_ALL_CHANGE_TO_RESPONSE
    result &= (g_appRxSwitchToResponse || !g_slaveAppResponseActive);
#endif // !ENABLE_ALL_CHANGE_TO_RESPONSE

    return result;
}


/// Processes the reading of up to the length byte in the app's response.
/// @param[in]  data    The app's response up to the length byte.
/// @param[in]  size    The number of bytes in data.
/// @return The AppRxLengthResult including the status and the size of the
///         data payload (additional data to receive).
static AppRxLengthResult processAppRxLength(uint8_t data[], uint8_t size)
{
    static uint8_t const CommandMask = 0x7f;
    static uint8_t const InvalidCommand = 0x00;
    
    AppRxLengthResult result = { { false }, 0u };
    if ((data != NULL) && (size >= G_AppRxPacketLengthSize))
    {
        result.dataPayloadSize = data[AppRxPacketOffset_Length];
        if (result.dataPayloadSize >= G_InvalidRxAppPacketLength)
            result.invalidLength = true;

        if ((data[AppRxPacketOffset_Command] & CommandMask) == InvalidCommand)
        {
            result.invalidCommand = true;
        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
            if (!g_appRxSwitchToResponse)
            {
                result.invalidAppBuffer = true;
                g_appRxSwitchToResponse = true;
            }
        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE    
            
        }
    }
    else
        result.invalidParameters = true;
    return result;
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
static uint32_t findExtendedTimeoutMs(uint16_t transactionSize)
{
    static uint32_t const WordSize = 9u;
    static uint32_t const PeriodUs = 10u;
    static uint32_t const Shift = 10u;
    static uint32_t const Adjustment = 1u;
    
    uint32_t extendedTimeoutMs = transactionSize * WordSize * PeriodUs;
    extendedTimeoutMs = (extendedTimeoutMs >> Shift) + Adjustment;
    return extendedTimeoutMs;
}


/// Check and update the driver status. No error processing or handling is done
/// in this function; the caller must perform approriate error handling.
/// @return The driver status mask.
static mstatus_t checkDriverStatus(void)
{
    g_lastDriverStatus = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    COMPONENT(SLAVE_I2C, I2CMasterClearStatus)();
    return g_lastDriverStatus;
}


/// Checks to see if an I2C-related error occurred. If an error occurred and an
/// error callback function has been registered, the error callback function
/// will be invoked.
/// @param[in]  status      Status indicating if an error occured. See the
///                         definition of the I2cStatus union.
static void processError(I2cStatus status)
{
    if (i2c_errorOccurred(status) && (g_errorCallback != NULL))
        g_errorCallback(status, g_callsite.value);
}


/// Processes any errors that may have occured in the last I2C transaction.
static I2cStatus processPreviousTranferErrors(mstatus_t status)
{
    static mstatus_t const PreviousDoneMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_RD_CMPLT) | COMPONENT(SLAVE_I2C, I2C_MSTAT_WR_CMPLT);
    static mstatus_t const ErrorMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_MASK);
    
    I2cStatus returnStatus = i2c_getNoErrorI2cStatus();
    if ((status & PreviousDoneMask) > 0)
    {
        if ((status & ErrorMask) > 0)
        {
            if ((status & COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_ADDR_NAK)) > 0)
                returnStatus.nak = true;
            returnStatus.driverError = true;
        }
    }
    return returnStatus;
}


/// Checks to see if the slave I2C bus is ready. Also handles errors caused by
/// previous transactions.
/// @param[out] status  Status indicating if an error occured. See the
///                     definition of the I2cStatus union.
/// @return If the bus is ready for a new read/write transaction.
static bool isBusReady(I2cStatus* status)
{
    static mstatus_t const BusyMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_INP) | COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_HALT);
    
    g_lastDriverStatus = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    COMPONENT(SLAVE_I2C, I2CMasterClearStatus)();
    bool ready = (g_lastDriverStatus & BusyMask) == 0;
    I2cStatus localStatus = processPreviousTranferErrors(g_lastDriverStatus);
    if (i2c_errorOccurred(localStatus))
    {
        g_callsite.isBusReady = true;
        processError(localStatus);
    }
    if (status != NULL)
        *status = localStatus;
    return ready;
}


/// Checks to see if the I2C bus is locked up. For the bus to be considered to
/// be locked up, the bus status must be busy for over a specific period of time
/// tracked by the locked bus alarm.
/// @return If the bus is locked.
static bool isBusLocked(void)
{
#if ENABLE_I2C_LOCKED_BUS_DETECTION
    bool locked = g_lockedBus.locked;
#else
    bool locked = false;
#endif // ENABLE_I2C_LOCKED_BUS_DETECTION
    return locked;
}


#if ENABLE_I2C_LOCKED_BUS_DETECTION
    
    /// Reset the locked bus structure to the default values. Also disables all
    /// alarms associated with the locked bus.
    static void resetLockedBusStructure(void)
    {
        alarm_disarm(&g_lockedBus.detectAlarm);
        alarm_disarm(&g_lockedBus.recoverAlarm);
        g_lockedBus.recoveryAttempts = 0;
        g_lockedBus.locked = false;
    }
    
#endif // ENABLE_I2C_LOCKED_BUS_DETECTIONS


/// Updates the driver status and generates the I2cStatus that corresponds
/// to the return result from the low-level driver function.
/// @param[in]  mode    The TransferMode flags used in the low-level driver
///                     function.
/// @param[in]  result  The result from the low-level driver function call.
I2cStatus updateDriverStatus(mreturn_t result)
{
    I2cStatus status = i2c_getNoErrorI2cStatus();
    if (result != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))            
    {
        status.driverError = true;
        if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
            status.nak = true;
        if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_TIMEOUT)) > 0)
            status.timedOut = true;
    #if ENABLE_I2C_LOCKED_BUS_DETECTION
        if ((result & (COMPONENT(SLAVE_I2C, I2C_MSTR_BUS_BUSY) | COMPONENT(SLAVE_I2C, I2C_MSTR_NOT_READY))) > 0)
        {
            g_lockedBus.locked = isBusLocked() ||
                (g_lockedBus.detectAlarm.armed && alarm_hasElapsed(&g_lockedBus.detectAlarm));
            status.lockedBus = g_lockedBus.locked;
            if (!g_lockedBus.detectAlarm.armed)
                alarm_arm(&g_lockedBus.detectAlarm, G_DefaultLockedBusDetectTimeoutMs, AlarmType_ContinuousNotification);
            if (g_lockedBus.locked && !g_lockedBus.recoverAlarm.armed)
                alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMs, AlarmType_ContinuousNotification);
        }
    #endif // ENABLE_I2C_LOCKED_BUS_DETECTION
    }
    else
    {
        if (isBusLocked())
            resetLockedBusStructure();
    }
    
    // Update the driver status.
    g_lastDriverStatus = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    COMPONENT(SLAVE_I2C, I2CMasterClearStatus)();
    
    return status;
}


/// Read data from a slave device on the I2C bus.
/// @param[in]  address
/// @param[out] data    Data buffer to copy the read data to.
/// @param[in]  size    The number of bytes to read; it's assumed that data is
///                     at least this length.
/// @param[in]  mode    TransferMode settings.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
static I2cStatus read(uint8_t address, uint8_t data[], uint16_t size)
{
    g_lastDriverReturnValue = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, G_DefaultTransferMode);
    I2cStatus status = updateDriverStatus(g_lastDriverReturnValue);
    if (i2c_errorOccurred(status))
        g_callsite.lowLevelCall = 1u;
    return status;
}


/// Write data to the slave device on the I2C bus.
/// @param[in]  address
/// @param[out] data    Data buffer to copy the read data to.
/// @param[in]  size    The number of bytes to read; it's assumed that data is
///                     at least this length.
/// @param[in]  mode    TransferMode settings.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
static I2cStatus write(uint8_t address, uint8_t const data[], uint16_t size)
{
    I2cStatus status;
    if ((data != NULL) && (size > 0))
    {
        // Note: typecast of data to (uint8_t*) to remove the const typing in
        // order to utilize the low-level driver function.
        g_lastDriverReturnValue = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, (uint8_t*)data, size, G_DefaultTransferMode);
        status = updateDriverStatus(g_lastDriverReturnValue);
    #if !ENABLE_ALL_CHANGE_TO_RESPONSE
        if (!i2c_errorOccurred(status))
        {
            if (address == g_slaveAddress)
                g_slaveAppResponseActive = (data[XferQueueDataOffset_Xfer] >= AppBufferOffset_Response);
        }
    #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
    }
    else
        status.invalidInputParameters = true;
    if (i2c_errorOccurred(status))
        g_callsite.lowLevelCall = 2u;
    return status;
}


#if ENABLE_I2C_LOCKED_BUS_DETECTION
    
    /// Attempts to recover from the bus lock error in the case that the I2C bus
    /// gets locked by either the SCL or SDA being held low for extended
    /// periods.
    /// See the following site for ideas on recovery:
    /// https://community.cypress.com/t5/PSoC-Creator-Designer-Software/Correct-way-to-reset-I2C-SCB-and-recover-stuck-bus/m-p/213188
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cStatus union.
    static I2cStatus recoverFromLockedBus(void)
    {
        I2cStatus status = i2c_getNoErrorI2cStatus();
        if (g_lockedBus.recoverAlarm.armed && alarm_hasElapsed(&g_lockedBus.recoverAlarm))
        {
            debug_setPin1(false);
            if (g_lockedBus.recoveryAttempts >= G_MaxRecoveryAttempts)
            {
                //CySoftwareReset();
            }
            // Rearm the alarm for the next attempt.
            alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMs, AlarmType_ContinuousNotification);
            
            #if true
            // First attempt to restart the I2C component.
            COMPONENT(SLAVE_I2C, Stop)();
            // Try to clear the status register.
            COMPONENT(SLAVE_I2C, I2C_STATUS_REG) = 0;
            // Init is called instead of Start b/c of the initialization flag in the
            // component has already been set.
            COMPONENT(SLAVE_I2C, Init)();
            COMPONENT(SLAVE_I2C, Enable)();
            status = i2c_ackApp(0);
            #endif
            g_lockedBus.recoveryAttempts++;
            debug_setPin1(true);
        }
        if (i2c_errorOccurred(status))
            g_callsite.recoverFromLockedBus = true;
        return status;
    }
    
#endif // ENABLE_I2C_LOCKED_BUS_DETECTION


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
static I2cStatus resetIrq(void)
{
    return write(g_slaveAddress, G_ClearIrqMessage, G_ClearIrqSize);
}


/// Changes the slave app to response buffer active state so responses can be
/// properly read from the slave device.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
static I2cStatus changeSlaveAppToResponseBuffer(void)
{
    return write(g_slaveAddress, G_ClearIrqMessage, G_ResponseBufferSize);
    // Note: The write function will change the flag to reflect if the app was
    // switched to the response buffer.
}


/// Communications finite state machine (FSM) to process any receive and
/// transmit transactions pertaining to the I2C bus.
/// @param[in]  timeoutMs   The amount of time the process can occur before it
///                         times out and must finish. If 0, then there's no
///                         timeout and the function blocks until all pending
///                         actions are completed.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
static I2cStatus processCommFsm(uint32_t timeoutMs)
{
    I2cStatus status = i2c_getNoErrorI2cStatus();
    if (timeoutMs > 0)
        alarm_arm(&g_commFsm.timeoutAlarm, timeoutMs, AlarmType_ContinuousNotification);
    else
        alarm_disarm(&g_commFsm.timeoutAlarm);
    
    // Determine the next state when waiting.
    if (g_commFsm.state == CommState_Waiting)
    {
        if (g_commFsm.rxPending && isIrqAsserted())
            g_commFsm.state = CommState_RxPending;
        else if (!queue_isEmpty(g_heap->queue))
            g_commFsm.state = CommState_XferDequeueAndAct;
    }
    
    while (g_commFsm.state != CommState_Waiting)
    {
        if (g_commFsm.timeoutAlarm.armed && alarm_hasElapsed(&g_commFsm.timeoutAlarm))
        {
            status.timedOut = true;
            g_commFsm.state = CommState_Waiting;
            break;
        }
        
        switch (g_commFsm.state)
        {
            case CommState_RxPending:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 1u;
                g_commFsm.rxPending = false;
                g_commFsm.rxSwitchToResponseBuffer = false;
                g_commFsm.pendingRxSize = G_AppRxPacketLengthSize;
                if (switchToAppResponseBuffer())
                {
                    g_commFsm.rxSwitchToResponseBuffer = true;
                    g_commFsm.state = CommState_RxSwitchToResponseBuffer;
                }
                else
                    g_commFsm.state = CommState_RxReadLength;
                break;
            }
            
            case CommState_RxSwitchToResponseBuffer:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 2u;
                if (isBusReady(&status))
                {
                    status = changeSlaveAppToResponseBuffer();
                    if (!i2c_errorOccurred(status))
                        g_commFsm.state = CommState_RxReadLength;
                    else
                        g_commFsm.state = CommState_Waiting;
                }
                break;
            }
            
            case CommState_RxReadLength:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 3u;
                if (isBusReady(&status))
                {
                    status = read(g_slaveAddress, g_heap->rxBuffer, g_commFsm.pendingRxSize);
                    if (!i2c_errorOccurred(status))
                        g_commFsm.state = CommState_RxProcessLength;
                    else
                        g_commFsm.state = CommState_Waiting;
                }
                break;
            }
            
            case CommState_RxProcessLength:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 4u;
                if (isBusReady(&status))
                {
                    AppRxLengthResult lengthResult = processAppRxLength(g_heap->rxBuffer, g_commFsm.pendingRxSize);
                    if (!lengthResult.invalid)
                    {
                        g_commFsm.pendingRxSize += lengthResult.dataPayloadSize;
                        if (lengthResult.dataPayloadSize <= 0)
                            g_commFsm.state = CommState_RxProcessExtraData;
                        else
                        {
                            alarm_snooze(&g_commFsm.timeoutAlarm, findExtendedTimeoutMs(g_commFsm.pendingRxSize));
                            g_commFsm.state = CommState_RxReadExtraData;
                        }
                    }
                    else if (lengthResult.invalidParameters)
                    {
                        status.invalidInputParameters = true;
                        g_commFsm.state = CommState_Waiting;
                    }
                    else
                    {
                        // No issue with the I2C transaction; there's an issue
                        // with the data, so still clear the IRQ. Also, set the
                        // next state first because depending on the length
                        // result, the next state will be different.
                        g_commFsm.state = CommState_RxClearIrq;
                        
                        if (lengthResult.invalidCommand)
                        {    
                        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
                            if (lengthResult.invalidAppBuffer)
                                g_commFsm.state = CommState_RxSwitchToResponseBuffer;
                            else
                        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
                            {
                                status.invalidRead = true;
                                // No issue with the I2C transaction; there's an issue
                                // with the data, so still clear the IRQ.
                                g_commFsm.state = CommState_RxClearIrq;
                            }
                        }
                        if (lengthResult.invalidLength)
                        {
                            if (!g_commFsm.rxSwitchToResponseBuffer)
                            {
                                g_commFsm.rxSwitchToResponseBuffer = true;
                                g_commFsm.state = CommState_RxSwitchToResponseBuffer;
                            }
                            else
                                status.invalidRead = true;
                        }
                    }
                }
                break;
            }
            
            case CommState_RxReadExtraData:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 5u;
                if (isBusReady(&status))
                {
                    status = read(g_slaveAddress, g_heap->rxBuffer, g_commFsm.pendingRxSize);
                    if (!i2c_errorOccurred(status))
                        g_commFsm.state = CommState_RxProcessExtraData;
                    else
                        g_commFsm.state = CommState_Waiting;
                }
                break;
            }
            
            case CommState_RxProcessExtraData:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 6u;
                if (isBusReady(&status))
                {
                    if (g_rxCallback != NULL)
                        g_rxCallback(g_heap->rxBuffer, g_commFsm.pendingRxSize);
                    g_commFsm.state = CommState_RxClearIrq;
                }
                break;
            }
            
            case CommState_RxClearIrq:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 7u;
                if (isBusReady(&status))
                {
                    status = resetIrq();
                    g_commFsm.state = CommState_RxCheckComplete;
                }
                break;
            }
            
            case CommState_RxCheckComplete:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 8u;
                if (isBusReady(&status))
                    g_commFsm.state = CommState_Waiting;
                break;
            }
            
            case CommState_XferDequeueAndAct:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 9u;
                if (isBusReady(&status))
                {
                    uint8_t* data;
                    uint16_t size = queue_dequeue(g_heap->queue, &data);
                    if (size > XferQueueDataOffset_Data)
                    {
                        g_commFsm.pendingRxSize = 0u;
                        I2cXfer xfer = { data[XferQueueDataOffset_Xfer] };
                        if (xfer.direction == I2cDirection_Write)
                        {
                            // Exclude the I2cXfer byte in the transmit size.
                            size--;
                            alarm_snooze(&g_commFsm.timeoutAlarm, findExtendedTimeoutMs(size));
                            status = write(xfer.address, &data[XferQueueDataOffset_Data], size);
                        }
                        else if (xfer.direction == I2cDirection_Read)
                        {
                            g_commFsm.pendingRxSize = data[XferQueueDataOffset_Data];
                            alarm_snooze(&g_commFsm.timeoutAlarm, findExtendedTimeoutMs(g_commFsm.pendingRxSize));
                            status = read(xfer.address, g_heap->rxBuffer, data[XferQueueDataOffset_Data]);
                        }
                        if (!i2c_errorOccurred(status))
                        {
                            // If pendingRxSize > 0, then a read is occurring,
                            // otherwise a write is occurring.
                            if (g_commFsm.pendingRxSize > 0)
                                g_commFsm.state = CommState_XferRxCheckComplete;
                            else
                                g_commFsm.state = CommState_XferTxCheckComplete;
                        }
                        else
                            g_commFsm.state = CommState_Waiting;
                    }
                    else
                    {
                        status.invalidInputParameters = true;
                        g_commFsm.state = CommState_Waiting;
                    }
                }
                break;
            }
            
            case CommState_XferRxCheckComplete:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 10u;
                if (isBusReady(&status))
                {
                    if (g_rxCallback != NULL)
                        g_rxCallback(g_heap->rxBuffer, g_commFsm.pendingRxSize);
                    g_commFsm.state = CommState_Waiting;
                }
                break;
            }
            
            case CommState_XferTxCheckComplete:
            {
                g_callsite.subValue = 0u;
                g_callsite.subCall = 10u;
                if (isBusReady(&status))
                    g_commFsm.state = CommState_Waiting;
                break;
            }
            
            default:
            {
                // Should never get here.
                g_callsite.subValue = 0u;
                g_callsite.subCall = 15u;
                alarm_disarm(&g_commFsm.timeoutAlarm);
                g_commFsm.state = CommState_Waiting;
            }
        }
        
        // The state machine can only be in the waiting state in the while loop
        // if it transitioned to it because the receive is complete. If this
        // occurs, disarm the alarm.
        if (g_commFsm.state == CommState_Waiting)
            alarm_disarm(&g_commFsm.timeoutAlarm);
    }
    return status;
}


/// Enqueue a read transaction into the transfer queue.
/// @param[in]  address The 7-bit I2C address.
/// @param[in]  size    The number of bytes to read.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
I2cStatus xferEnqueueRead(uint8_t address, uint16_t size)
{
    I2cStatus status = i2c_getNoErrorI2cStatus();
    if (g_heap != NULL)
    {
        if ((size > 0) && (size <= UINT8_MAX))
        {
            if (!queue_isFull(g_heap->queue))
            {
                uint8_t readSize = size;
                g_heap->pendingQueueXfer.address = address;
                g_heap->pendingQueueXfer.direction = I2cDirection_Read;
                if (!queue_enqueue(g_heap->queue, &readSize, sizeof(readSize)))
                    status.queueFull = true;
            }
            else
                status.queueFull = true;
        }
        else
            status.invalidInputParameters = true;
    }
    else
        status.deactivated = true;
    return status;
}


/// Enqueue a write transaction into the transfer queue.
/// @param[in]  address The 7-bit I2C address.
/// @param[in]  data    The data buffer to that contains the data to write.
/// @param[in]  size    The number of bytes to write.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
I2cStatus xferEnqueueWrite(uint8_t address, uint8_t const data[], uint16_t size)
{
    I2cStatus status = i2c_getNoErrorI2cStatus();
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (!queue_isFull(g_heap->queue))
            {
                g_heap->pendingQueueXfer.address = address;
                g_heap->pendingQueueXfer.direction = I2cDirection_Write;
                if (!queue_enqueue(g_heap->queue, data, size))
                    status.queueFull = true;
            }
            else
                status.queueFull = true;
        }
        else
            status.invalidInputParameters = true;
    }
    else
        status.deactivated = true;
    return status;
}


/// Checks if the last I2C transfer completed. The status will indicate if there
/// was a transfer error.
/// @param[out] status  Status indicating if an error occured. See the
///                     definition of the I2cStatus union.
/// @return If the tranfer completed.
bool isLastTransferComplete(I2cStatus* status)
{
    bool complete = false;
    mstatus_t driverStatus = checkDriverStatus();
    if (driverStatus == COMPONENT(SLAVE_I2C, I2C_MSTAT_CLEAR))
        complete = true;
    if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_RD_CMPLT)) > 0)
    {
        if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_ADDR_NAK)) > 0)
            status->nak = true;
        else if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_MASK)) > 0)
            status->driverError = true;
        complete = true;
    }
    return complete;
}


/// Performs an I2C slave ACK: attempt to read one byte from a specific slave
/// address. If the slave address exists, the address byte will be acknowledged.
/// Note that this is a blocking function.
/// @param[in]  address The 7-bit I2C address.
/// @param[in]  timeout The amount of time in milliseconds the function can wait
///                     for the I2C bus to free up before timing out. If 0, then
///                     the function will wait for a default timeout period.
/// @return Status indicating if an error occured. See the definition of the
///         I2cStatus union.
I2cStatus ack(uint8_t address, uint32_t timeoutMs)
{
    static uint32_t const DefaultAckTimeout = 2u;
    
    Alarm alarm;
    if (timeoutMs <= 0)
        timeoutMs = DefaultAckTimeout;
    alarm_arm(&alarm, timeoutMs, AlarmType_ContinuousNotification);
    
    bool ackSent = false;
    bool done = false;
    I2cStatus status = i2c_getNoErrorI2cStatus();
    while (!done)
    {
        if (alarm.armed && alarm_hasElapsed(&alarm))
        {
            status.timedOut = true;
            break;
        }
        
        if (!ackSent)
        {
            // Dummy byte used so that the I2C read function has a valid
            // non-NULL pointer for reading 0 bytes.
            uint8_t dummy;
            if (isBusReady(NULL))
            {
                status = read(address, &dummy, sizeof(dummy));
                if (i2c_errorOccurred(status))
                    done = true;
                else
                    ackSent = true;
            }
        }
        else
        {
            // Check the driver status, block until the transaction is done.
            done = isLastTransferComplete(&status);
        }
    }
    return status;
}


/// Resets the comm finite state machine to the default/starting condition.
static void resetCommFsm(void)
{
    alarm_disarm(&g_commFsm.timeoutAlarm);
    g_commFsm.pendingRxSize = 0u;
    g_commFsm.rxPending = false;
    g_commFsm.rxSwitchToResponseBuffer = false;
    g_commFsm.state = CommState_Waiting;
}


/// Resets the slave status flags to the default states. The following flags
/// are reset:
/// 1. g_slaveAppResponseActive (false)
/// 2. g_slaveNoStop (false)
static void resetSlaveStatusFlags(void)
{
#if !ENABLE_ALL_CHANGE_TO_RESPONSE
    
    g_slaveAppResponseActive = false;
    g_appRxSwitchToResponse = false;
    
#endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
}


/// Reinitializes/resets all variables, states, and alarms to the default
/// conditions.
static void reinitAll(void)
{
    resetCommFsm();
    resetSlaveStatusFlags();
#if ENABLE_I2C_LOCKED_BUS_DETECTION
    resetLockedBusStructure();
#endif // ENABLE_I2C_LOCKED_BUS_DETECTION
}


/// Initializes the heap when in touch mode.
/// @param[in]  heap    Pointer to the specific touch heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initTouchHeap(TouchHeap* heap)
{
    queue_registerEnqueueCallback(&heap->heapData.xferQueue, prepareXferQueueData);
    heap->heapData.xferQueue.data = heap->heapData.xferQueueData;
    heap->heapData.xferQueue.elements = heap->heapData.xferQueueElements;
    heap->heapData.xferQueue.maxDataSize = XFER_QUEUE_DATA_SIZE;
    heap->heapData.xferQueue.maxSize = XFER_QUEUE_MAX_SIZE;
    queue_empty(&heap->heapData.xferQueue);
    g_heap->queue = &heap->heapData.xferQueue;
    g_heap->rxBuffer = heap->heapData.rxBuffer;
    g_heap->rxBufferSize = TOUCH_RX_BUFFER_SIZE;
}


/// Initializes the heap when in update mode.
/// @param[in]  heap    Pointer to the specific touch heap data structure that
///                     defines the address offset for the heap data,
///                     specifically the queue data.
static void initUpdateHeap(UpdateHeap* heap)
{
    g_heap->queue = NULL;
    g_heap->rxBuffer = heap->heapData.rxBuffer;
    g_heap->rxBufferSize = UPDATE_RX_BUFFER_SIZE;
}


/// General deactivation function that applies to both the touch and update
/// heap variants.
/// @return If the module had to be deactivated. Returns false if the module
///         was already deactivated.
static bool deactivate(void)
{
    bool deactivate = false;
    if (g_heap != NULL)
    {
        g_heap = NULL;
        deactivate = true;
    }
    reinitAll();
    return deactivate;
}


// === ISR =====================================================================

/// ISR for the slaveIRQ (for the slaveIRQPin). The IRQ is asserted when there's
/// pending I2C data to be read from the I2C slave.
CY_ISR(slaveIsr)
{
    COMPONENT(SLAVE_IRQ, ClearPending)();
    COMPONENT(SLAVE_IRQ_PIN, ClearInterrupt)();
    g_commFsm.rxPending = true;
}


// === PUBLIC FUNCTIONS ========================================================

void i2c_init(void)
{
    reinitAll();
    i2c_resetSlaveAddress();
    
    COMPONENT(SLAVE_I2C, Start)();
    COMPONENT(SLAVE_IRQ, StartEx)(slaveIsr);
}



void i2c_setSlaveAddress(uint8_t address)
{
    if (address != g_slaveAddress)
    {
        g_slaveAddress = address;
        reinitAll();
    }
}


void i2c_resetSlaveAddress(void)
{
    if (g_slaveAddress != SlaveAddress_App)
    {
        g_slaveAddress = SlaveAddress_App;
        reinitAll();
    }
}


uint16_t i2c_getLastDriverStatusMask(void)
{
    return (uint16_t)g_lastDriverStatus;
}


uint16_t i2c_getLastDriverReturnValue(void)
{
    return g_lastDriverReturnValue;
}

    
void i2c_registerRxCallback(I2cRxCallback callback)
{
    if (callback != NULL)
        g_rxCallback = callback;
}


void i2c_registerErrorCallback(I2cErrorCallback callback)
{
    if (callback != NULL)
        g_errorCallback = callback;
}


I2cStatus i2c_ack(uint8_t address, uint32_t timeoutMs)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 4u;
    
    I2cStatus status = ack(address, timeoutMs);
    processError(status);
    return status;
}


I2cStatus i2c_ackApp(uint32_t timeoutMs)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 5u;
    
    I2cStatus status = ack(g_slaveAddress, timeoutMs);
    processError(status);
    return status;
}


I2cStatus i2c_read(uint8_t address, uint8_t data[], uint16_t size, uint32_t timeoutMs)
{
    Alarm alarm;
    if (timeoutMs <= 0)
        timeoutMs = findExtendedTimeoutMs(size);
    alarm_arm(&alarm, timeoutMs, AlarmType_ContinuousNotification);
    
    bool sent = false;
    bool done = false;
    I2cStatus status = i2c_getNoErrorI2cStatus();
    while (!done)
    {
        if (alarm.armed && alarm_hasElapsed(&alarm))
        {
            status.timedOut = true;
            break;
        }
        
        if (!sent)
        {
            if (isBusReady(NULL))
            {
                status = read(address, data, size);
                if (i2c_errorOccurred(status))
                    done = true;
                else
                    sent = true;
            }
        }
        else
        {
            // Check the driver status, block until the transaction is done.
            done = isLastTransferComplete(&status);
        }
    }
    processError(status);
    return status;
}


I2cStatus i2c_write(uint8_t address, uint8_t const data[], uint16_t size, uint32_t timeoutMs)
{
    Alarm alarm;
    if (timeoutMs <= 0)
        timeoutMs = findExtendedTimeoutMs(size);
    alarm_arm(&alarm, timeoutMs, AlarmType_ContinuousNotification);
    
    bool sent = false;
    bool done = false;
    I2cStatus status = i2c_getNoErrorI2cStatus();
    while (!done)
    {
        if (alarm.armed && alarm_hasElapsed(&alarm))
        {
            status.timedOut = true;
            break;
        }
        
        if (!sent)
        {
            if (isBusReady(NULL))
            {
                status = write(address, data, size);
                if (i2c_errorOccurred(status))
                    done = true;
                else
                    sent = true;
            }
        }
        else
        {
            // Check the driver status, block until the transaction is done.
            done = isLastTransferComplete(&status);
        }
    }
    processError(status);
    return status;
}


bool i2c_errorOccurred(I2cStatus const status)
{
    return (status.mask != G_NoErrorI2cStatus.mask);
}


I2cStatus i2c_getNoErrorI2cStatus(void)
{
    return G_NoErrorI2cStatus;
}


// === PUBLIC FUNCTIONS: i2cTouch ==============================================

uint16_t i2cTouch_getHeapWordRequirement(void)
{
    return heap_calculateHeapWordRequirement(sizeof(TouchHeap));
}


uint16_t i2cTouch_activate(heapWord_t memory[], uint16_t size)
{
    uint16_t allocatedSize = 0;
    uint16_t requiredSize = i2cTouch_getHeapWordRequirement();
    if ((memory != NULL) && (size >= requiredSize))
    {
        g_heap = (Heap*)memory;
        TouchHeap* heap = (TouchHeap*)g_heap;
        initTouchHeap(heap);
        allocatedSize = requiredSize;
        reinitAll();
    }
    return allocatedSize;
}
 

uint16_t i2cTouch_deactivate(void)
{
    uint16_t size = 0u;
    if (deactivate())
        size = i2cTouch_getHeapWordRequirement();
    return size;
}


bool i2cTouch_isActivated(void)
{
    return ((g_heap != NULL) && (g_heap->queue != NULL));
}


I2cStatus i2cTouch_process(uint32_t timeoutMs)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 1u;
    
    I2cStatus status = i2c_getNoErrorI2cStatus();
    
#if ENABLE_I2C_LOCKED_BUS_DETECTION
    if (isBusLocked())
        status = recoverFromLockedBus();
    else
#endif // ENABLE_I2C_LOCKED_BUS_DETECTION
    {
        if (g_heap != NULL)
            status = processCommFsm(timeoutMs);
        else
            status.deactivated = true;
    }
    processError(status);
    return status;
}


I2cStatus i2cTouch_read(uint8_t address, uint16_t size)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 2u;
    
    I2cStatus status = xferEnqueueRead(address, size);
    processError(status);
    return status;
}


I2cStatus i2cTouch_write(uint8_t address, uint8_t const data[], uint16_t size)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 3u;
    
    I2cStatus status = xferEnqueueWrite(address, data, size);
    processError(status);
    return status;
}


// === PUBLIC FUNCTIONS: i2cUpdate =============================================


uint16_t i2cUpdate_getHeapWordRequirement(void)
{
    return heap_calculateHeapWordRequirement(sizeof(UpdateHeap));
}


uint16_t i2cUpdate_activate(heapWord_t memory[], uint16_t size)
{
    uint16_t allocatedSize = 0;
    uint16_t requiredSize = i2cUpdate_getHeapWordRequirement();
    if ((memory != NULL) && (size >= requiredSize))
    {
        g_heap = (Heap*)memory;
        UpdateHeap* heap = (UpdateHeap*)g_heap;
        initUpdateHeap(heap);
        allocatedSize = requiredSize;
        reinitAll();
    }
    return allocatedSize;
}


uint16_t i2cUpdate_deactivate(void)
{
    uint16_t size = 0u;
    if (deactivate())
        size = i2cUpdate_getHeapWordRequirement();
    return size;
}


bool i2cUpdate_isActivated(void)
{
    return ((g_heap != NULL) && (g_heap->queue == NULL));
}


I2cStatus i2cUpdate_read(uint8_t address, uint8_t data[], uint16_t size, uint32_t timeoutMs)
{
    return i2c_read(address, data, size, timeoutMs);
}


I2cStatus i2cUpdate_write(uint8_t address, uint8_t const data[], uint16_t size, uint32_t timeoutMs)
{
    return i2c_write(address, data, size, timeoutMs);
}


I2cStatus i2cUpdate_bootloaderRead(uint8_t data[], uint16_t size, uint32_t timeoutMs)
{
    return i2c_read(SlaveAddress_Bootloader, data, size, timeoutMs);
}


I2cStatus i2cUpdate_bootloaderWrite(uint8_t const data[], uint16_t size, uint32_t timeoutMs)
{
    return i2c_write(SlaveAddress_Bootloader, data, size, timeoutMs);
}


/* [] END OF FILE */
