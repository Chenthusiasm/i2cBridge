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

/// Enable/disable checking if on slave IRQ, if a write to change to the slave
/// app's response buffer must be done before reading.
/// true:   always change to the response buffer on every interrupt.
/// false:  check the contents of the read to determine if the switch to
///         response buffer must always be done on interrupt.
#define ENABLE_ALL_CHANGE_TO_RESPONSE   (false)

/// Enable/disable the optimized transfer mode to indicate for low-level I2C
/// read/write to send restart and to not send the stop condition. This will
/// allow for optimized reads which uses no stops and restarts in the I2C
/// transactions to hold onto the bus and potentially reduce I2C byte traffic.
#define ENABLE_OPTIMIZED_TRANSFER_MODE  (false)

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

/// The master driver level function status type. The I2CMasterStatus function
/// returns a uint32_t, but the returned value only uses 16-bits at most.
typedef uint16_t mstatus_t;

/// The master driver level function return type. The I2CMasterWriteBuf,
/// I2CMasterReadBuf, and I2CMasterSendStop functions return a uint32_t. Only
/// the I2CMasterSendStop function requires the return type to be uint32_t due
/// to the timeout error. The other two functions' return type can fit in a
/// uint16_t. Therefore, the return type is defined as the larger uint32_t.
typedef uint32_t mreturn_t;

#if ENABLE_OPTIMIZED_TRANSFER_MODE
    
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
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE


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
    /// The state machine is waiting for pending data to be read from the slave.
    AppRxState_Waiting,
    
    /// Data to be read from the slave is pending.
    AppRxState_Pending,
    
    /// App needs to be switched to the response buffer being active.
    AppRxState_SwitchToResponseBuffer,
    
    /// Read the length of the response.
    AppRxState_ReadLength,
    
    /// Process the length after reading.
    AppRxState_ProcessLength,
    
    /// Read the remaining data payload state.
    AppRxState_ReadDataPayload,
    
    /// Process the the data payload after reading.
    AppRxState_ProcessDataPayload,
    
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    /// Complete the read operation by sending a stop.
    AppRxState_StopRead,
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE
    
    /// Clear the IRQ.
    AppRxState_ClearIrq,
    
    /// Attempt to recover the I2C bus after the bus has been determined to be
    /// locked-up.
    AppRxState_RecoverFromLockedBus,
    
} AppRxState;


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


/// Application receive state machine variables.
typedef struct AppRxStateMachine
{
    /// Timeout alarm used to determine if the I2C receive transaction has timed
    /// out.
    Alarm timeoutAlarm;
    
    /// Number of bytes that need to be received (pending).
    uint16_t pendingRxSize;
    
    /// The current state. This needs to be volatile because it is modified in
    /// an ISR.
    volatile AppRxState state;
    
} AppRxStateMachine;


/// Locked bus variables.
typedef struct LockedBus
{
    /// Alarm to determine after how long since a continuous bus busy error from
    /// the low level driver should the status flag be set indicating a locked
    /// bus.
    Alarm detectAlarm;
    
    /// Alarm to track when to attempt a recovery after a locked bus has been
    /// detected.
    Alarm recoverAlarm;
    
    /// Track the number of recovery attempts since the locked bus was detected.
    /// This can be used to determine when to trigger a system reset.
    uint8_t recoveryAttempts;
    
    /// Flag indicating if the bus is in the locked condition.
    bool locked;
    
} LockedBus;


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

#if !ENABLE_OPTIMIZED_TRANSFER_MODE
    
    /// Default transfer mode mask for the low-level I2C driver read and write
    /// functions.
    static uint8_t const G_DefaultTransferMode = COMPONENT(SLAVE_I2C, I2C_MODE_COMPLETE_XFER);
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE

/// Default timeout for the alarm for detecting a locked bus condition.
static uint32_t const G_DefaultLockedBusDetectTimeoutMS = 200u;

/// Default timeout for the alarm used to determine how often to attempt to
/// recover from the locked bus.
static uint32_t const G_DefaultLockedBusRecoveryPeriodMS = 100u;

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
static AppRxStateMachine g_appRxStateMachine;

/// Container for locked-bus related variables.
static LockedBus g_lockedBus;

#if !ENABLE_ALL_CHANGE_TO_RESPONSE
    
    /// Flag indicating we're in the response buffer is active for the slave app.
    static bool g_slaveAppResponseActive = false;
    
    /// Flag indicating on receive, if a write needs to be done to switch to the
    /// response buffer.
    static bool g_appRxSwitchToResponse = false;
    
#endif // !ENABLE_ALL_CHANGE_TO_RESPONSE

#if ENABLE_OPTIMIZED_TRANSFER_MODE
    
    /// Flag indicating that the slave is in the middle of a transaction and no
    /// stop was sent; the bus will remain busy.
    static bool g_slaveNoStop = false;
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE

/// The receive callback function.
static I2cGen2_RxCallback g_rxCallback = NULL;

/// The error callback function.
static I2cGen2_ErrorCallback g_errorCallback = NULL;

/// The status of the last driver API call. Refer to the possible error messages
/// in the generated "I2C Component Name"_I2C.h file. Note that the return type
/// of the function to get the status is uint32_t, but in actuality, only a
/// uint16_t is returned.
static mstatus_t g_lastDriverStatus = 0;

/// The return value of the last driver API call. Refer to possible errors in
/// the generated "I2C Component Name"_I2C.h file.
static mreturn_t g_lastDriverReturnValue = 0;


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


/// Checks to see if the slave I2C bus is ready.
/// @return If the bus is ready for a new read/write transaction.
static bool isBusReady(void)
{
    g_lastDriverStatus = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterStatus)();
    COMPONENT(SLAVE_I2C, I2CMasterClearStatus)();
    return (
    #if ENABLE_OPTIMIZED_TRANSFER_MODE
        g_slaveNoStop ||
    #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
        (g_lastDriverStatus & COMPONENT(SLAVE_I2C, I2C_MSTAT_XFER_INP)) == 0);
}


/// Checks to see if the I2C bus is locked up. For the bus to be considered to
/// be locked up, the bus status must be busy for over a specific period of time
/// tracked by the locked bus alarm.
/// @return If the bus is locked.
static bool isBusLocked(void)
{
    bool locked = g_lockedBus.locked;
    if (locked)
        debug_setPin1(false);
    else
        debug_setPin1(true);
    return locked;
}


/// Reset the locked bus structure to the default values. Also disables all
/// alarms associated with the locked bus.
static void resetLockedBusStructure(void)
{
    alarm_disarm(&g_lockedBus.detectAlarm);
    alarm_disarm(&g_lockedBus.recoverAlarm);
    g_lockedBus.recoveryAttempts = 0;
    g_lockedBus.locked = false;
}


/// Updates the driver status and generates the I2cGen2Status that corresponds
/// to the return result from the low-level driver function.
/// @param[in]  mode    The TransferMode flags used in the low-level driver
///                     function.
/// @param[in]  result  The result from the low-level driver function call.
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    I2cGen2Status updateDriverStatus(TransferMode mode, mreturn_t result)
#else
    I2cGen2Status updateDriverStatus(mreturn_t result)
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE
    {
        I2cGen2Status status = { false };
        if (result != COMPONENT(SLAVE_I2C, I2C_MSTR_NO_ERROR))            
        {
            status.driverError = true;
            if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_LB_NAK)) > 0)
                status.nak = true;
            if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_ERR_TIMEOUT)) > 0)
                status.timedOut = true;
            if ((result & COMPONENT(SLAVE_I2C, I2C_MSTR_BUS_BUSY)) > 0)
            {
                g_lockedBus.locked = isBusLocked() ||
                    (g_lockedBus.detectAlarm.armed && alarm_hasElapsed(&g_lockedBus.detectAlarm));
                status.lockedBus = g_lockedBus.locked;
                if (!g_lockedBus.detectAlarm.armed)
                    alarm_arm(&g_lockedBus.detectAlarm, G_DefaultLockedBusDetectTimeoutMS, AlarmType_ContinuousNotification);
                if (g_lockedBus.locked && !g_lockedBus.recoverAlarm.armed)
                    alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMS, AlarmType_ContinuousNotification);
            }
            else
                resetLockedBusStructure();
        }
    #if ENABLE_OPTIMIZED_TRANSFER_MODE
        else
            g_slaveNoStop = mode.noStop;
    #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
        
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
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    
    static I2cGen2Status read(uint8_t address, uint8_t data[], uint16_t size, TransferMode mode)
    {
        g_lastDriverReturnValue = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, mode.value);
        return updateDriverStatus(mode, g_lastDriverReturnValue);
    }
    
#else
    
    static I2cGen2Status read(uint8_t address, uint8_t data[], uint16_t size)
    {
        g_lastDriverReturnValue = COMPONENT(SLAVE_I2C, I2CMasterReadBuf)(address, data, size, G_DefaultTransferMode);
        return updateDriverStatus(g_lastDriverReturnValue);
    }
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE


/// Write data to the slave device on the I2C bus.
/// @param[in]  address
/// @param[out] data    Data buffer to copy the read data to.
/// @param[in]  size    The number of bytes to read; it's assumed that data is
///                     at least this length.
/// @param[in]  mode    TransferMode settings.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    static I2cGen2Status write(uint8_t address, uint8_t const data[], uint16_t size, TransferMode mode)
#else
    static I2cGen2Status write(uint8_t address, uint8_t const data[], uint16_t size)
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE
    {
        I2cGen2Status status;
        if ((data != NULL) && (size > 0))
        {
            // Note: remove the const typing in order to utilize the low-level
            // driver function.
        #if ENABLE_OPTIMIZED_TRANSFER_MODE
            g_lastDriverReturnValue = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, (uint8_t*)data, size, mode.value);
            status = updateDriverStatus(mode, g_lastDriverReturnValue);
        #else
            g_lastDriverReturnValue = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterWriteBuf)(address, (uint8_t*)data, size, G_DefaultTransferMode);
            status = updateDriverStatus(g_lastDriverReturnValue);
        #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
            if (!status.errorOccurred)
            {
                if (address == g_slaveAddress)
                    g_slaveAppResponseActive = (data[TxQueueDataOffset_Address] >= AppBufferOffset_Response);
            }
        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
        }
        else
            status.inputParametersInvalid = true;
        return status;
    }


#if ENABLE_OPTIMIZED_TRANSFER_MODE
    
    /// Sends the stop condition on the I2C bus. Only call this if a previous read
    /// or write call was called with the TransferMode.noStop flag was set to true.
    /// @return Status indicating if an error occured. See the definition of the
    ///         I2cGen2Status union.
    static I2cGen2Status sendStop(void)
    {
        // Dummy TransferMode constant to pass into the updateDriverStatus function.
        static TransferMode const mode = { { false, false } };
        g_lastDriverReturnValue = (uint16_t)COMPONENT(SLAVE_I2C, I2CMasterSendStop)(G_DefaultSendStopTimeoutMS);
        I2cGen2Status status = updateDriverStatus(mode, g_lastDriverReturnValue);
        if (!status.errorOccurred)
            g_slaveNoStop = false;
        return status;
    }
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE


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
        // First perform a simple read of the slave device to determine if the
        // bus is still locked.
        uint8_t dummy;
        status = read(g_slaveAddress, &dummy, sizeof(dummy));
        if (g_lockedBus.locked)
        {
            if (g_lockedBus.recoveryAttempts >= G_MaxRecoveryAttempts)
            {
                debug_setPin1(true);
                debug_setPin1(false);
                debug_setPin1(true);
                debug_setPin1(false);
                debug_setPin1(true);
                debug_setPin1(false);
                debug_setPin1(true);
                debug_setPin1(false);
            }
            
            debug_setPin1(true);
            
            // Rearm the alarm for the next attempt.
            alarm_arm(&g_lockedBus.recoverAlarm, G_DefaultLockedBusRecoveryPeriodMS, AlarmType_ContinuousNotification);
            
            #if (false)
            // First attempt to restart the I2C component.
            COMPONENT(SLAVE_I2C, Stop)();
            // Try to clear the status register.
            COMPONENT(SLAVE_I2C, I2C_STATUS_REG) = 0;
            // Init is called instead of Start b/c of the initialization flag in the
            // component has already been set.
            COMPONENT(SLAVE_I2C, Init)();
            COMPONENT(SLAVE_I2C, Enable)();
            
            // Recheck if a read works.
            status = read(g_slaveAddress, &dummy, sizeof(dummy));
            if (status.busStuck)
            {
                // Reconfigure the SDA and SCL lines to GPIO and attempt to manually
                // reset the bus by clocking in clock signals.
                ;
            }
            #endif
            debug_setPin1(false);
        }
        else
            resetLockedBusStructure();
    }
    return status;
}


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status resetIrq(void)
{
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    TransferMode mode = { { false, false } };
    return write(g_slaveAddress, G_ClearIrqMessage, G_ClearIrqSize, mode);
#else
    return write(g_slaveAddress, G_ClearIrqMessage, G_ClearIrqSize);
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE
}


/// Changes the slave app to response buffer active state so responses can be
/// properly read from the slave device.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status changeSlaveAppToResponseBuffer(void)
{
#if ENABLE_OPTIMIZED_TRANSFER_MODE
    TransferMode mode = { { false, false } };
    return write(g_slaveAddress, G_ClearIrqMessage, G_ResponseBufferSize, mode);
#else
    return write(g_slaveAddress, G_ClearIrqMessage, G_ResponseBufferSize);
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE
    // Note: The write function will change the flag to reflect if the app was
    // switched to the response buffer.
}


/// State machine to process the IRQ indicating that the slave app has data
/// ready to be received by the host.
/// @param[in]  timeoutMS   The amount of time the process can occur before it
///                         times out and must finish. If 0, then there's no
///                         timeout and the function blocks until all pending
///                         actions are completed.
/// @return Status indicating if an error occured. See the definition of the
///         I2cGen2Status union.
static I2cGen2Status processAppRxStateMachine(uint32_t timeoutMS)
{
    I2cGen2Status status = { false };
    
    if (timeoutMS > 0)
        alarm_arm(&g_appRxStateMachine.timeoutAlarm, timeoutMS, AlarmType_ContinuousNotification);
    else
        alarm_disarm(&g_appRxStateMachine.timeoutAlarm);
        
    while (g_appRxStateMachine.state != AppRxState_Waiting)
    {
        // Handling the bus busy alarm expiring takes precedence over any other
        // action in the state machine.
        if (isBusLocked())
        {
            g_appRxStateMachine.state = AppRxState_RecoverFromLockedBus;
        }
        else if (g_appRxStateMachine.timeoutAlarm.armed && alarm_hasElapsed(&g_appRxStateMachine.timeoutAlarm))
        {
            status.timedOut = true;
            g_appRxStateMachine.state = AppRxState_Waiting;
            break;
        }
        
        switch (g_appRxStateMachine.state)
        {
            case AppRxState_Pending:
            {
                g_appRxStateMachine.pendingRxSize = G_AppRxPacketLengthSize;
                if (switchToAppResponseBuffer())
                    g_appRxStateMachine.state = AppRxState_SwitchToResponseBuffer;
                else
                    g_appRxStateMachine.state = AppRxState_ReadLength;
                break;
            }
            
            case AppRxState_SwitchToResponseBuffer:
            {
                if (isBusReady())
                {
                    status = changeSlaveAppToResponseBuffer();
                    if (!status.errorOccurred)
                        g_appRxStateMachine.state = AppRxState_ReadLength;
                    else
                        g_appRxStateMachine.state = AppRxState_Waiting;
                }
                break;
            }
            
            case AppRxState_ReadLength:
            {
                if (isBusReady())
                {
                #if ENABLE_OPTIMIZED_TRANSFER_MODE
                    TransferMode mode = { { false, false } };
                    status = read(g_slaveAddress, g_heap->rxBuffer, g_appRxStateMachine.pendingRxSize, mode);
                #else
                    status = read(g_slaveAddress, g_heap->rxBuffer, g_appRxStateMachine.pendingRxSize);
                #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                    if (!status.errorOccurred)
                        g_appRxStateMachine.state = AppRxState_ProcessLength;
                    else
                        g_appRxStateMachine.state = AppRxState_Waiting;
                }
                break;
            }
            
            case AppRxState_ProcessLength:
            {
                if (isBusReady())
                {
                    AppRxLengthResult lengthResult = processAppRxLength(g_heap->rxBuffer, g_appRxStateMachine.pendingRxSize);
                    if (!lengthResult.invalid)
                    {
                    #if ENABLE_OPTIMIZED_TRANSFER_MODE
                        g_appRxStateMachine.pendingRxSize = lengthResult.dataPayloadSize;
                    #else
                        g_appRxStateMachine.pendingRxSize += lengthResult.dataPayloadSize;
                    #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                        if (lengthResult.dataPayloadSize <= 0)
                            g_appRxStateMachine.state = AppRxState_ProcessDataPayload;
                        else
                        {
                            alarm_snooze(&g_appRxStateMachine.timeoutAlarm, findExtendedTimeoutMS(g_appRxStateMachine.pendingRxSize));
                            g_appRxStateMachine.state = AppRxState_ReadDataPayload;
                        }
                    }
                    else if (lengthResult.invalidParameters)
                    {
                        status.inputParametersInvalid = true;
                        g_appRxStateMachine.state = AppRxState_Waiting;
                    }
                    else
                    {
                        if (lengthResult.invalidCommand)
                        {    
                        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
                            if (lengthResult.invalidAppBuffer)
                            {
                                g_appRxStateMachine.state = AppRxState_SwitchToResponseBuffer;
                                continue;
                            }
                            else
                        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
                            {
                                status.invalidRead = true;
                                // No issue with the I2C transaction; there's an issue
                                // with the data, so still clear the IRQ.
                                g_appRxStateMachine.state = AppRxState_ClearIrq;
                            }
                        }
                        if (lengthResult.invalidLength)
                            status.invalidRead = true;
                            
                        // No issue with the I2C transaction; there's an issue
                        // with the data, so still clear the IRQ.
                        g_appRxStateMachine.state = AppRxState_ClearIrq;
                    }
                }
                break;
            }
            
            case AppRxState_ReadDataPayload:
            {
                if (isBusReady())
                {
                #if ENABLE_OPTIMIZED_TRANSFER_MODE
                    TransferMode mode = { { false, false } };
                    status = read(g_slaveAddress, &g_heap->rxBuffer[AppRxPacketOffset_Data], g_appRxStateMachine.pendingRxSize, mode);
                #else
                    status = read(g_slaveAddress, g_heap->rxBuffer, g_appRxStateMachine.pendingRxSize);
                #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                    
                    if (!status.errorOccurred)
                        g_appRxStateMachine.state = AppRxState_ProcessDataPayload;
                    else
                        g_appRxStateMachine.state = AppRxState_Waiting;
                }
                break;
            }
            
            case AppRxState_ProcessDataPayload:
            {
                if (isBusReady())
                {
                #if ENABLE_OPTIMIZED_TRANSFER_MODE
                    uint16_t length = G_AppRxPacketLengthSize + g_appRxStateMachine.pendingRxSize;
                #else
                    uint16_t length = g_appRxStateMachine.pendingRxSize;
                #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                    if (g_rxCallback != NULL)
                        g_rxCallback(g_heap->rxBuffer, length);
                #if ENABLE_OPTIMIZED_TRANSFER_MODE
                    g_appRxStateMachine.state = AppRxState_StopRead;
                #else
                    g_appRxStateMachine.state = AppRxState_ClearIrq;
                #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                }
                break;
            }
            
        #if ENABLE_OPTIMIZED_TRANSFER_MODE
            case AppRxState_StopRead:
            {
                if (isBusReady())
                {
                    status = sendStop();
                    if (!status.errorOccurred)
                        g_appRxStateMachine.state = AppRxState_ClearIrq;
                    else
                        g_appRxStateMachine.state = AppRxState_Waiting;
                }
                break;
            }
        #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
            
            case AppRxState_ClearIrq:
            {
                if (isBusReady())
                {
                    status = resetIrq();
                    g_appRxStateMachine.state = AppRxState_Waiting;
                }
                break;
            }
            
            case AppRxState_RecoverFromLockedBus:
            {
                status = recoverFromLockedBus();
                g_appRxStateMachine.state = AppRxState_Waiting;
                break;
            }
            
            default:
            {
                // Should never get here.
                alarm_disarm(&g_appRxStateMachine.timeoutAlarm);
                g_appRxStateMachine.state = AppRxState_Waiting;
            }
        }
        
        // The state machine can only be in the waiting state in the while loop
        // if it transitioned to it because the receive is complete. If this
        // occurs, disarm the alarm.
        if (g_appRxStateMachine.state == AppRxState_Waiting)
            alarm_disarm(&g_appRxStateMachine.timeoutAlarm);
    }
    return status;
}


/// Checks to see if an I2C-related error occurred. If an error occurred and an
/// error callback function has been registered, the error callback function
/// will be invoked.
/// @param[in]  status      Status indicating if an error occured. See the
///                         definition of the I2cGen2Status union.
/// @param[in]  callsite    Unique callsite ID to distinguish different
///                         functions that had an I2C error.
static void processError(I2cGen2Status status, uint16_t callsite)
{
    if (status.errorOccurred && (g_errorCallback != NULL))
        g_errorCallback(status, callsite);
}


/// Resets the app receive state machine to the default/starting condition.
static void resetAppRxStateMachine(void)
{
    g_appRxStateMachine.state = AppRxState_Waiting;
    g_appRxStateMachine.pendingRxSize = 0u;
    alarm_disarm(&g_appRxStateMachine.timeoutAlarm);
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

#if ENABLE_OPTIMIZED_TRANSFER_MODE
    
    g_slaveNoStop = false;
    
#endif // ENABLE_OPTIMIZED_TRANSFER_MODE

}


/// Reinitializes/resets all variables, states, and alarms to the default
/// conditions.
static void reinitAll(void)
{
    resetAppRxStateMachine();
    resetSlaveStatusFlags();
    resetLockedBusStructure();
}


// === ISR =====================================================================

/// ISR for the slaveIRQ (for the slaveIRQPin). The IRQ is asserted when there's
/// pending I2C data to be read from the I2C slave.
CY_ISR(slaveIsr)
{
    COMPONENT(SLAVE_IRQ, ClearPending)();
    COMPONENT(SLAVE_IRQ_PIN, ClearInterrupt)();
        
    g_appRxStateMachine.state = AppRxState_Pending;
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


bool i2cGen2_processRx(uint32_t timeoutMS)
{
    static uint16_t Callsite = 0x0100;
    
    bool result = false;
    I2cGen2Status status = { false };
    if (isBusLocked())
        status = recoverFromLockedBus();
    else
    {
        if (g_heap != NULL)
        {
            if (g_appRxStateMachine.state != AppRxState_Waiting)
            {
                if (isIrqAsserted())
                {
                    status = processAppRxStateMachine(timeoutMS);
                    if (!status.errorOccurred)
                        result = true;
                }
                else
                    g_appRxStateMachine.state = AppRxState_Waiting;
            }
        }
        else
            status.deactivated = true;
    }
    processError(status, Callsite);
    return result;
}


int i2cGen2_processTxQueue(uint32_t timeoutMS, bool quitIfBusy)
{
    static uint16_t Callsite = 0x0200;
    
    int count = 0;
    I2cGen2Status status = { false };
    if (isBusLocked())
        status = recoverFromLockedBus();
    else
    {
        if (g_heap != NULL)
        {
            Alarm alarm;
            if (timeoutMS > 0)
                alarm_arm(&alarm, timeoutMS, AlarmType_ContinuousNotification);
            else
                alarm_disarm(&alarm);
                
            int count = 0;
            while (!queue_isEmpty(&g_heap->txQueue))
            {
                if (alarm.armed && alarm_hasElapsed(&alarm))
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
        {
            status.deactivated = true;
            count = -1;
        }
    }
    processError(status, Callsite);
    return count;
}


I2cGen2Status i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size)
{
    static uint16_t const Callsite = 0x0300;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (isBusReady())
            {
            #if ENABLE_OPTIMIZED_TRANSFER_MODE
                TransferMode mode = { { false, false } };
                status = read(address, data, size, mode);
            #else
                status = read(address, data, size);
            #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
            }
            else
                status.timedOut = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    static uint16_t const Callsite = 0x0400;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > 0))
        {
            if (isBusReady())
            {
            #if ENABLE_OPTIMIZED_TRANSFER_MODE
                TransferMode mode = { { false, false } };
                status = write(address, data, size, mode);
            #else
                status = write(address, data, size);
            #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
            }
            else
                status.timedOut = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size)
{
    // Note: the error processing works differently in this function because it
    // calls i2cGen2_write which has its own error processing; only process
    // errors if i2cGen2_write is not invoked.
    
    static uint16_t const Callsite = 0x0500;
    static uint8_t const MinSize = 2u;
    static uint8_t const AddressOffset = 0u;
    static uint8_t const DataOffset = 1u;
    
    I2cGen2Status status = { false };
    bool invokedWrite = false;
    if (g_heap != NULL)
    {
        if ((data != NULL) && (size > MinSize))
        {
            size--;
            status = i2cGen2_write(data[AddressOffset], &data[DataOffset], size);
            invokedWrite = true;
        }
        else
            status.inputParametersInvalid = true;
    }
    else
        status.deactivated = true;
    if (invokedWrite)
        processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_txEnqueue(uint8_t address, uint8_t data[], uint16_t size)
{
    static uint16_t const Callsite = 0x0600;
    
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
    processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_txEnqueueWithAddressInData(uint8_t data[], uint16_t size)
{
    static uint16_t const Callsite = 0x0700;
    
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
    processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_ack(uint8_t address, uint32_t timeoutMS)
{
    static uint16_t const Callsite = 0x0800;
    static uint32_t const DefaultAckTimeout = 2u;
    
    I2cGen2Status status = { false };
    if (g_heap != NULL)
    {
        Alarm alarm;
        if (timeoutMS <= 0)
            timeoutMS = DefaultAckTimeout;
        alarm_arm(&alarm, timeoutMS, AlarmType_ContinuousNotification);
        
        bool acknowledged = false;
        while (!acknowledged)
        {
            if (alarm.armed && alarm_hasElapsed(&alarm))
            {
                status.timedOut = true;
                break;
            }
            
            // Dummy byte used so that the I2C read function has a valid
            // non-NULL pointer for reading 0 bytes.
            uint8_t dummy;
            if (isBusReady())
            {
            #if ENABLE_OPTIMIZED_TRANSFER_MODE
                TransferMode mode = { { false, false } };
                status = read(address, &dummy, sizeof(dummy), mode);
            #else
                status = read(address, &dummy, sizeof(dummy));
            #endif // ENABLE_OPTIMIZED_TRANSFER_MODE
                acknowledged = !status.errorOccurred;
            }
        }
    }
    else
        status.deactivated = true;
    processError(status, Callsite);
    return status;
}


I2cGen2Status i2cGen2_ackApp(uint32_t timeoutMS)
{
    return i2cGen2_ack(g_slaveAddress, timeoutMS);
}


/* [] END OF FILE */
