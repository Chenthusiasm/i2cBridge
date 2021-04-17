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
#include "error.h"
#include "project.h"
#include "queue.h"
#include "utility.h"


// === DEFINES =================================================================

/// Enable/disable the locked I2C bus detection and recovery.
#define ENABLE_LOCKED_BUS_DETECTION     (true)

/// Enable/disable checking if on slave IRQ, if a write to change to the slave
/// app's response buffer must be done before reading.
/// true:   always change to the response buffer on every interrupt.
/// false:  check the contents of the read to determine if the switch to
///         response buffer must always be done on interrupt.
#define ENABLE_ALL_CHANGE_TO_RESPONSE   (false)

/// Name of the slave I2C component.
#define SLAVE_I2C                       slaveI2c_

/// Name of the slave IRQ component.
#define SLAVE_IRQ                       slaveIrq_

/// Name of the slave IRQ pin component.
#define SLAVE_IRQ_PIN                   slaveIrqPin_

/// Size of the raw receive data buffer.
#define RX_BUFFER_SIZE                  (260u)

/// The max size of the host transfer queue (the max number of queue elements).
#define HOST_XFER_QUEUE_MAX_SIZE        (8u)

/// The size of the data array that holds the queue element data in the host
/// transfer queue.
#define HOST_XFER_QUEUE_DATA_SIZE       (600u)


// === TYPE DEFINES ============================================================

/// The master driver level function status type. The I2CMasterStatus function
/// returns a uint32_t, but the returned value only uses 16-bits at most.
typedef uint16_t mstatus_t;

/// The master driver level function return type. The I2CMasterWriteBuf,
/// I2CMasterReadBuf, and I2CMasterSendStop functions return a uint32_t. Only
/// the I2CMasterSendStop function requires the return type to be uint32_t due
/// to the timeout error. The other two functions' return type can fit in a
/// uint16_t. Therefore, the return type is defined as the larger uint32_t.
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
typedef enum HostXferQueueDataOffset
{
    /// The I2C address.
    HostXferQueueDataOffset_Xfer        = 0u,
    
    /// The start of the data payload.
    HostXferQueueDataOffset_Data        = 1u,
    
} HostXferQueueDataOffset;


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
    
    /// Check if the last transfer queue transaction has completed.
    CommState_XferCheckComplete,
    
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


#if ENABLE_LOCKED_BUS_DETECTION
    
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

#endif // ENABLE_LOCKED_BUS_DETECTION


/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap
{
    /// Host transfer queue.
    Queue hostXferQueue;
    
    /// Array of host transfer queue elements for the host transfer queue.
    QueueElement hostXferQueueElements[HOST_XFER_QUEUE_MAX_SIZE];
    
    /// Array to hold the data of the elements in the host transfer queue. Note
    /// that each transfer queue element has at least 2 bytes:
    /// [0]: I2cXfer (adress and direction)
    /// [1]:
    ///     read: number of bytes to read.
    ///     write: data payload...
    uint8_t hostXferQueueData[HOST_XFER_QUEUE_DATA_SIZE];
    
    /// The raw receive buffer.
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    
    /// The I2C address and direction associated with the transaction that is
    /// waiting to be enqueued into the transfer queue. This must be set prior
    /// to enqueueing data into the transfer queue.
    I2cXfer pendingQueueXfer;
    
} Heap;


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


// === CONSTANTS ===============================================================

/// The number of bytes to read in order to find the number of bytes in the
/// receive data payload.
static uint8_t const G_AppRxPacketLengthSize = AppRxPacketOffset_Length + 1u;

/// The value of the length byte which indicates the packet is invalid.
static uint8_t const G_InvalidRxAppPacketLength = 0xff;

/// The default amount of time to allow for a I2C stop condition to be sent
/// before timing out. If a time out occurs, the I2C module is reset. This is
/// in milliseconds.
static uint32_t const G_DefaultSendStopTimeoutMS = 5u;

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
static uint32_t const G_DefaultLockedBusDetectTimeoutMS = 100u;

/// Default timeout for the alarm used to determine how often to attempt to
/// recover from the locked bus.
static uint32_t const G_DefaultLockedBusRecoveryPeriodMS = 50u;

/// Max number of recovery attempts before performing a system reset.
static uint8_t const G_MaxRecoveryAttempts = 10u;


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

/// App receive state machine variables.
static CommFsm g_commFsm;

#if ENABLE_LOCKED_BUS_DETECTION
    
    /// Container for locked-bus related variables.
    static LockedBus g_lockedBus;
    
#endif // ENABLE_LOCKED_BUS_DETECTION

#if !ENABLE_ALL_CHANGE_TO_RESPONSE
    
    /// Flag indicating we're in the response buffer is active for the slave app.
    static bool g_slaveAppResponseActive = false;
    
    /// Flag indicating on receive, if a write needs to be done to switch to the
    /// response buffer.
    static bool g_appRxSwitchToResponse = false;
    
#endif // !ENABLE_ALL_CHANGE_TO_RESPONSE

/// The receive callback function.
static I2cGen2_RxCallback g_rxCallback = NULL;

/// The error callback function.
static I2cGen2_ErrorCallback g_errorCallback = NULL;

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
    static uint16_t const MinSourceSize = HostXferQueueDataOffset_Xfer + 1u;
    
    uint16_t size = 0;
    if ((source != NULL) && (sourceSize >= MinSourceSize) && (target != NULL) && (targetSize > sourceSize))
    {
        target[size++] = g_heap->pendingQueueXfer.value;
        memcpy(&target[size], source, sourceSize);
        size += sourceSize;
    }
    return size;
}

/// Initializes the transmit queue.
static void initTxQueue(void)
{
    queue_registerEnqueueCallback(&g_heap->hostXferQueue, prepareXferQueueData);
    g_heap->hostXferQueue.data = g_heap->hostXferQueueData;
    g_heap->hostXferQueue.elements = g_heap->hostXferQueueElements;
    g_heap->hostXferQueue.maxDataSize = HOST_XFER_QUEUE_DATA_SIZE;
    g_heap->hostXferQueue.maxSize = HOST_XFER_QUEUE_MAX_SIZE;
    queue_empty(&g_heap->hostXferQueue);
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
///                         definition of the I2cGen2Status union.
static void processError(I2cGen2Status status)
{
    if (status.errorOccurred && (g_errorCallback != NULL))
        g_errorCallback(status, g_callsite.value);
}


/// Processes any errors that may have occured in the last I2C transaction.
static I2cGen2Status processPreviousTranferErrors(mstatus_t status)
{
    static mstatus_t const PreviousDoneMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_RD_CMPLT) | COMPONENT(SLAVE_I2C, I2C_MSTAT_WR_CMPLT);
    static mstatus_t const ErrorMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_MASK);
    
    I2cGen2Status returnStatus = { false };
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
///                     definition of the I2cGen2Status union.
/// @return If the bus is ready for a new read/write transaction.
static bool isBusReady(I2cGen2Status* status)
{
    static mstatus_t const BusyMask = COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_INP) | COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_HALT);
    
    g_lastDriverStatus = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    COMPONENT(SLAVE_I2C, I2CMasterClearStatus)();
    bool ready = (g_lastDriverStatus & BusyMask) == 0;
    I2cGen2Status localStatus = processPreviousTranferErrors(g_lastDriverStatus);
    if (localStatus.errorOccurred)
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
#if ENABLE_LOCKED_BUS_DETECTION
    bool locked = g_lockedBus.locked;
#else
    bool locked = false;
#endif // ENABLE_LOCKED_BUS_DETECTION
    if (locked)
        debug_setPin1(false);
    else
        debug_setPin1(true);
    return locked;
}


#if ENABLE_LOCKED_BUS_DETECTION
    
    /// Reset the locked bus structure to the default values. Also disables all
    /// alarms associated with the locked bus.
    static void resetLockedBusStructure(void)
    {
        debug_setPin0(false);
        alarm_disarm(&g_lockedBus.detectAlarm);
        alarm_disarm(&g_lockedBus.recoverAlarm);
        g_lockedBus.recoveryAttempts = 0;
        g_lockedBus.locked = false;
        debug_setPin0(true);
    }
    
#endif // ENABLE_LOCKED_BUS_DETECTIONS


/// Updates the driver status and generates the I2cGen2Status that corresponds
/// to the return result from the low-level driver function.
/// @param[in]  mode    The TransferMode flags used in the low-level driver
///                     function.
/// @param[in]  result  The result from the low-level driver function call.
I2cGen2Status updateDriverStatus(mreturn_t result)
{
    I2cGen2Status status = { false };
    if (result != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))            
    {
        status.driverError = true;
        if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
            status.nak = true;
        if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_TIMEOUT)) > 0)
            status.timedOut = true;
    #if ENABLE_LOCKED_BUS_DETECTION
        if ((result & (COMPONENT(SLAVE_I2C, I2C_MSTR_BUS_BUSY) | COMPONENT(SLAVE_I2C, I2C_MSTR_NOT_READY))) > 0)
        {
            g_lockedBus.locked = isBusLocked() ||
                (g_lockedBus.detectAlarm.armed && alarm_hasElapsed(&g_lockedBus.detectAlarm));
            status.lockedBus = g_lockedBus.locked;
            if (!g_lockedBus.detectAlarm.armed)
                alarm_arm(&g_lockedBus.detectAlarm, G_DefaultLockedBusDetectTimeoutMS, AlarmType_ContinuousNotification);
            if (g_lockedBus.locked && !g_lockedBus.recoverAlarm.armed)
                alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMS, AlarmType_ContinuousNotification);
        }
    #endif // ENABLE_LOCKED_BUS_DETECTION
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
///         I2cGen2Status union.
static I2cGen2Status read(uint8_t address, uint8_t data[], uint16_t size)
{
    g_lastDriverReturnValue = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, G_DefaultTransferMode);
    I2cGen2Status status = updateDriverStatus(g_lastDriverReturnValue);
    if (status.errorOccurred)
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
///         I2cGen2Status union.
static I2cGen2Status write(uint8_t address, uint8_t const data[], uint16_t size)
{
    I2cGen2Status status;
    if ((data != NULL) && (size > 0))
    {
        // Note: remove the const typing in order to utilize the low-level
        // driver function.
        g_lastDriverReturnValue = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, (uint8_t*)data, size, G_DefaultTransferMode);
        status = updateDriverStatus(g_lastDriverReturnValue);
    #if !ENABLE_ALL_CHANGE_TO_RESPONSE
        if (!status.errorOccurred)
        {
            if (address == g_slaveAddress)
                g_slaveAppResponseActive = (data[HostXferQueueDataOffset_Xfer] >= AppBufferOffset_Response);
        }
    #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
    }
    else
        status.invalidInputParameters = true;
    if (status.errorOccurred)
        g_callsite.lowLevelCall = 2u;
    return status;
}


#if ENABLE_LOCKED_BUS_DETECTION
    
    /// Attempts to recover from the bus lock error in the case that the I2C bus
    /// gets locked by either the SCL or SDA being held low for extended periods.
    /// See the following site for ideas on recovery:
    /// https://community.cypress.com/t5/PSoC-Creator-Designer-Software/Correct-way-to-reset-I2C-SCB-and-recover-stuck-bus/m-p/213188
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    static I2cGen2Status recoverFromLockedBus(void)
    {
        I2cGen2Status status = { false };
        if (g_lockedBus.recoverAlarm.armed && alarm_hasElapsed(&g_lockedBus.recoverAlarm))
        {
            if (g_lockedBus.recoveryAttempts >= G_MaxRecoveryAttempts)
            {
                debug_setPin1(true);
                debug_setPin1(false);
                debug_setPin1(true);
                debug_setPin1(false);
                //CySoftwareReset();
            }
            
            debug_setPin1(true);
            
            // Rearm the alarm for the next attempt.
            alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMS, AlarmType_ContinuousNotification);
            
            #if true
            // First attempt to restart the I2C component.
            COMPONENT(SLAVE_I2C, Stop)();
            // Try to clear the status register.
            COMPONENT(SLAVE_I2C, I2C_STATUS_REG) = 0;
            // Init is called instead of Start b/c of the initialization flag in the
            // component has already been set.
            COMPONENT(SLAVE_I2C, Init)();
            COMPONENT(SLAVE_I2C, Enable)();
            status = i2cGen2_ackApp(0);
            #endif
            g_lockedBus.recoveryAttempts++;
            debug_setPin1(false);
        }
        if (status.errorOccurred)
            g_callsite.recoverFromLockedBus = true;
        return status;
    }
    
#endif // ENABLE_LOCKED_BUS_DETECTION


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status resetIrq(void)
{
    return write(g_slaveAddress, G_ClearIrqMessage, G_ClearIrqSize);
}


/// Changes the slave app to response buffer active state so responses can be
/// properly read from the slave device.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status changeSlaveAppToResponseBuffer(void)
{
    return write(g_slaveAddress, G_ClearIrqMessage, G_ResponseBufferSize);
    // Note: The write function will change the flag to reflect if the app was
    // switched to the response buffer.
}


/// Communications finite state machine (FSM) to process any receive and
/// transmit transactions pertaining to the I2C bus.
/// @param[in]  timeoutMS   The amount of time the process can occur before it
///                         times out and must finish. If 0, then there's no
///                         timeout and the function blocks until all pending
///                         actions are completed.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status processCommFsm(uint32_t timeoutMS)
{
    I2cGen2Status status = { false };
    if (timeoutMS > 0)
        alarm_arm(&g_commFsm.timeoutAlarm, timeoutMS, AlarmType_ContinuousNotification);
    else
        alarm_disarm(&g_commFsm.timeoutAlarm);
    
    // Determine the next state when waiting.
    if (g_commFsm.state == CommState_Waiting)
    {
        if (g_commFsm.rxPending && isIrqAsserted())
            g_commFsm.state = CommState_RxPending;
        else if (!queue_isEmpty(&g_heap->hostXferQueue))
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
                    if (!status.errorOccurred)
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
                    if (!status.errorOccurred)
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
                            alarm_snooze(&g_commFsm.timeoutAlarm, findExtendedTimeoutMS(g_commFsm.pendingRxSize));
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
                    if (!status.errorOccurred)
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
                    uint16_t length = g_commFsm.pendingRxSize;
                    if (g_rxCallback != NULL)
                        g_rxCallback(g_heap->rxBuffer, length);
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
                    uint16_t size = queue_dequeue(&g_heap->hostXferQueue, &data);
                    if (size > HostXferQueueDataOffset_Data)
                    {
                        I2cXfer xfer = { data[HostXferQueueDataOffset_Xfer] };
                        if (xfer.direction == I2cDirection_Write)
                            status = write(xfer.address, &data[HostXferQueueDataOffset_Data], size - 1u);
                        else if (xfer.direction == I2cDirection_Read)
                            status = read(xfer.address, g_heap->rxBuffer, data[HostXferQueueDataOffset_Data]);
                        g_commFsm.state = CommState_XferCheckComplete;
                    }
                    else
                    {
                        status.invalidInputParameters = true;
                        g_commFsm.state = CommState_Waiting;
                    }
                }
                break;
            }
            
            case CommState_XferCheckComplete:
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


/// Resets the app receive state machine to the default/starting condition.
static void resetAppRxStateMachine(void)
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
    resetAppRxStateMachine();
    resetSlaveStatusFlags();
#if ENABLE_LOCKED_BUS_DETECTION
    resetLockedBusStructure();
#endif // ENABLE_LOCKED_BUS_DETECTION
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

void i2cGen2_init(void)
{
    reinitAll();
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
        reinitAll();
    }
    return allocatedSize;
}
 

void i2cGen2_deactivate(void)
{
    g_heap = NULL;
    reinitAll();
}


void i2cGen2_setSlaveAddress(uint8_t address)
{
    if (address != g_slaveAddress)
    {
        g_slaveAddress = address;
        reinitAll();
    }
}


void i2cGen2_resetSlaveAddress(void)
{
    if (g_slaveAddress != SlaveAddress_App)
    {
        g_slaveAddress = SlaveAddress_App;
        reinitAll();
    }
}


uint16_t i2cGen2_getLastDriverStatusMask(void)
{
    return (uint16_t)g_lastDriverStatus;
}


uint16_t i2cGen2_getLastDriverReturnValue(void)
{
    return g_lastDriverReturnValue;
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


I2cGen2Status i2cGen2_process(uint32_t timeoutMS)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 1u;
    
    I2cGen2Status status = { false };
    
#if ENABLE_LOCKED_BUS_DETECTION
    if (isBusLocked())
        status = recoverFromLockedBus();
    else
#endif // ENABLE_LOCKED_BUS_DETECTION
    {
        if (g_heap != NULL)
            status = processCommFsm(timeoutMS);
        else
            status.deactivated = true;
    }
    processError(status);
    return status;
}


I2cGen2Status i2cGen2_read(uint8_t address, uint16_t size)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 253u;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((size > 0) && (size <= UINT8_MAX))
        {
            if (!queue_isFull(&g_heap->hostXferQueue))
            {
                uint8_t readSize = size;
                g_heap->pendingQueueXfer.address = address;
                g_heap->pendingQueueXfer.direction = I2cDirection_Read;
                if (!queue_enqueue(&g_heap->hostXferQueue, &readSize, sizeof(readSize)))
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
    processError(status);
    return status;
}


I2cGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    g_callsite.value = 0u;
    g_callsite.topCall = 3u;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (!queue_isFull(&g_heap->hostXferQueue))
            {
                g_heap->pendingQueueXfer.address = address;
                g_heap->pendingQueueXfer.direction = I2cDirection_Write;
                if (!queue_enqueue(&g_heap->hostXferQueue, data, size))
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
    processError(status);
    return status;
}


I2cGen2Status i2cGen2_ack(uint8_t address, uint32_t timeoutMS)
{
    static uint32_t const DefaultAckTimeout = 2u;
    
    g_callsite.value = 0u;
    g_callsite.topCall = 5u;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS <= 0)
            timeoutMS = DefaultAckTimeout;
        alarm_arm(&alarm, timeoutMS, AlarmType_ContinuousNotification);
        
        bool ackSent = false;
        bool done = false;
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
                    if (status.errorOccurred)
                        done = true;
                    else
                        ackSent = true;
                }
            }
            else
            {
                // Check the driver status, block until the transaction is done.
                mstatus_t driverStatus = checkDriverStatus();
                if (driverStatus == COMPONENT(SLAVE_I2C, I2C_MSTAT_CLEAR))
                    done = true;
                if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_RD_CMPLT)) > 0)
                {
                    if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_ADDR_NAK)) > 0)
                        status.nak = true;
                    else if ((driverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_ERR_MASK)) > 0)
                        status.driverError = true;
                    done = true;
                }
            }
        }
    }
    else
        status.deactivated = true;
    processError(status);
    return status;
}


I2cGen2Status i2cGen2_ackApp(uint32_t timeoutMS)
{
    return i2cGen2_ack(g_slaveAddress, timeoutMS);
}


/* [] END OF FILE */
