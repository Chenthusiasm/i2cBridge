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

#include "bridgeFsm.h"

#include "alarm.h"
#include "debug.h"
#include "error.h"
#include "heap.h"
#include "i2cTouch.h"
#include "project.h"
#include "smallPrintf.h"
#include "uartCommon.h"
#include "utility.h"


// === DEFINES =================================================================

/// Name of the slave reset component.
#define SLAVE_RESET                     slaveReset_

/// The size of the heap for "dynamic" memory allocation in bytes.
#define HEAP_BYTE_SIZE                  (2800u)

/// The size of the heap for "dynamic" memory allocation. Note that the heap
/// contains data of type heapWord_t to keep word aligned; therefore, the byte
/// size is divided by the size of the heapWord_t. In the case of a 32-bit MCU,
/// the heapWord_t is a uint32_t (see heapWord_t).
#define HEAP_SIZE                       (HEAP_BYTE_SIZE / sizeof(heapWord_t))

/// The size of the error message buffer (for use with smallSprintf). Do not
/// make generic error messages larger than this, otherwise a buffer overflow
/// will occur.
#define ERROR_MESSAGE_BUFFER_SIZE       (64u)


// === TYPE DEFINES ============================================================

/// The different states of the state machine.
typedef enum State
{
    /// Initializes the host communication.
    State_InitHostComm,
    
    /// Initializes slave reset.
    State_InitSlaveReset,
    
    /// Initialize the slave translator mode.
    State_InitSlaveTranslator,
    
    /// Initialize the slave update mode.
    State_InitSlaveUpdate,
    
    /// Checks if the slave reset is complete.
    State_CheckSlaveResetComplete,
    
    /// Processes tasks related to the default I2C slave translator mode.
    State_SlaveTranslator,
    
    /// Processes tasks related to the I2C slave update mode.
    State_SlaveUpdate,
    
    /// The slave translator failed to initialize. Send a generic error message
    /// to the host.
    State_SlaveTranslatorFailed,
    
    /// The slave update failed to initialize. Send a generic error message to
    /// the host.
    State_SlaveUpdateFailed,
    
    /// Host communication failed to initialize. Send a generic error message
    /// to the host.
    State_HostCommFailed,
    
} State;


/// Union that provides flags to indicate if a bridge finite state machine mode
/// change needs to be done (pending).
typedef union ModeChange
{
    /// Flag indicating that a mode change is pending.
    bool pending;
    
    /// Anonymous struct that encapsulates different bits indicating if a mode
    /// change is requested.
    struct
    {
        /// Change to touch firmware translator mode is pending.
        bool translatorPending : 1;
        
        /// Change to touch firmware update mode is pending.
        bool updatePending : 1;
        
        /// Reset request is pending.
        bool resetPending : 1;
        
    };
    
} ModeChange;


/// General status of an allocation unit in the heap.
typedef struct HeapUnitStatus
{
    /// The size of the communication module heap (in words);
    uint16_t size;
    
    /// The offset in the heap for the communication module (in words).
    uint16_t offset;
    
    /// Flag indicating if the communication module is active.
    bool active;
    
} HeapUnitStatus;


/// Data structure that encapsulates the heap.
typedef struct Heap
{
    /// The buffer where "dynamically" allocated memory units are located.
    heapWord_t data[HEAP_SIZE];
    
    /// The offset into the heap that indicates the start of free space.
    uint16_t freeOffset;
    
} Heap;


// === CONSTANTS ===============================================================

/// The default period between writing of error messages to the host UART bus
/// when a general error occurs.
static uint32_t G_ErrorMessagePeriodMS = 5000u;


// === GLOBAL VARIABLES ========================================================

/// The current state of the state machine.
static State g_state = State_InitHostComm;

/// Flags indicating if a mode change was requested and is pending action. Also
/// indicates the specific mode.
static ModeChange g_modeChange = { false };

/// An alarm used to indicate how long to hold the slave device in reset.
static Alarm g_resetAlarm;

/// Heap data structure used for "dynamic" memory allocation.
static Heap g_heap;


// === PRIVATE FUNCTIONS =======================================================

/// Checks if the slave is resetting.
/// Note, the slave reset is configured as an open drain drives low GPIO. The
/// slave device's XRES line has an internal pull-up and is an active low reset.
/// @return If the slave is currently resetting (active low).
bool isSlaveResetting(void)
{
    return (COMPONENT(SLAVE_RESET, Read)() <= 0);
}


/// Puts the slave in reset or takes it out of reset, depending on the reset
/// input parameter.
/// @param[in]  reset   Flag indicating if the slave should be put in reset or
///                     not.
void resetSlave(bool reset)
{
    COMPONENT(SLAVE_RESET, Write)((reset) ? (0u) : (1u));
}


/// Get the remaining size in words of the heap, free for memory allocation.
/// @return The size, in words, that is free in the heap.
uint16_t getFreeHeapWordSize(void)
{
    return (HEAP_SIZE - g_heap.freeOffset);
}


/// Processes any system errors that may have occurred.
void processError(SystemStatus status)
{
    if (status.errorOccurred)
        error_tally(ErrorType_System);
}


/// Resets the heap to the default value. Additionally deactivates/deallocates
/// the heaps used by the host and slave communications.
/// @return Status indicating if an error occured. See the definition of the
///         SystemStatus union.
SystemStatus resetHeap(void)
{
    SystemStatus status = { false };
    
    // Deactivate/deallocate the communication sub systems if they're activated.
    uint16_t deactivationSize = 0;
    if (i2cTouch_isActivated())
        deactivationSize += i2cTouch_deactivate();
    if (uartCommon_isActivated() || uartCommon_isUpdateActivated())
        deactivationSize += uartCommon_deactivate();
        
    if (deactivationSize != g_heap.freeOffset)
        status.memoryLeak = true;
    g_heap.freeOffset = 0u;
    return status;
}


/// Initializes the host comm in translator mode.
/// @return Status indicating if an error occured. See the definition of the
///         SystemStatus union.
SystemStatus initHostComm(void)
{
    static bool const EnableUpdate = false;
    
    SystemStatus status = { false };
    if (uartCommon_isUpdateActivated())
        status = resetHeap();
    if (!status.errorOccurred)
    {
        if (!uartCommon_isActivated())
        {
            uint16_t size = uartCommon_activate(
                &g_heap.data[g_heap.freeOffset],
                getFreeHeapWordSize(), EnableUpdate);
            debug_uartPrintHexUint16(size);
            if (size > 0)
                g_heap.freeOffset += size;
            else
            {
                status.invalidScratchOffset = true;
                uint16_t requirement = uartCommon_getHeapWordRequirement(EnableUpdate);
                if (getFreeHeapWordSize() < requirement)
                    status.invalidScratchBuffer = true;
            }
            debug_uartPrintHexUint16(g_heap.freeOffset);
        }
    }
    return status;
}


/// Initializes the host communication bus.
/// @return If host was initialized.
bool processInitHostComm(void)
{
    SystemStatus status = initHostComm();
    processError(status);
    return !status.errorOccurred;;
}


/// Initialize the slave reset.
/// @return If the slave reset was initialized successfully.
bool processInitSlaveReset(void)
{
    SystemStatus status = { false };
    if (!isSlaveResetting())
    {
        static uint32_t const DefaultResetTimeoutMS = 100u;
        alarm_arm(&g_resetAlarm, DefaultResetTimeoutMS, AlarmType_ContinuousNotification);
        resetSlave(true);
    }
    else
        status.slaveResetFailed = true;
    return !status.errorOccurred;
}


/// Processes checking if the slave reset is complete.
/// @return If the slave reset completed.
bool processSlaveResetComplete(void)
{
    bool complete = false;
    SystemStatus status = { false };
    if (!g_resetAlarm.armed || alarm_hasElapsed(&g_resetAlarm))
    {
        resetSlave(false);
        CyDelayUs(50u);
        if (!isSlaveResetting())
            complete = true;
        else
            status.slaveResetFailed = true;
        alarm_disarm(&g_resetAlarm);
    }
    processError(status);
    return complete;
}


/// Processes all tasks associated with initializing the I2C slave translator.
/// @return If the initialization was successful.
bool processInitSlaveTranslator(void)
{
    SystemStatus status = initHostComm();
    if (!status.errorOccurred)
    {
        uint16_t size = i2cTouch_activate(&g_heap.data[g_heap.freeOffset], getFreeHeapWordSize());
        debug_uartPrintHexUint16(size);
        if (size <= 0)
        {
            status.invalidScratchOffset = true;
            uint16_t requirement = i2cTouch_getHeapWordRequirement();
            if (getFreeHeapWordSize() < requirement)
                status.invalidScratchBuffer = true;
            status.translatorError = true;
        }
        else
            g_heap.freeOffset += size;
        debug_uartPrintHexUint16(g_heap.freeOffset);
    }
    processError(status);
    return !status.errorOccurred;
}


/// Processes all tasks associated with the I2C slave translator.
/// @return If the translator processed successfully.
bool processSlaveTranslator(void)
{
    bool processed = false;
    if (true)
    {
        processed = true;
        uint32_t const UartProcessRxTimeoutMS = 2u;
        uint32_t const UartProcessTxTimeoutMS = 3u;
        uint32_t const I2cProcessTimeoutMS = 5u;
        
        uartCommon_processRx(UartProcessRxTimeoutMS);
        i2cTouch_process(I2cProcessTimeoutMS);
        uartCommon_processTx(UartProcessTxTimeoutMS);
    }
    return processed;
}


/// Processes all tasks associated with initializing the I2C slave update.
/// @return If the initialization was successful.
bool processInitSlaveUpdate(void)
{
    // @TODO: implement.
    return true;
}


/// Processes all tasks associated with the I2C slave update.
/// @return If the update processed successfully.
bool processSlaveUpdate(void)
{
    // @TODO: implement.
    return true;
}


/// Processes the case where the slave translator initialization failed. The
/// function will intermittently transmit an ASCII error message over the host
/// UART bus.
void processHostTranslatorFailed(void)
{
    static char const* ErrorMessage = "ERROR: slave translator failed init!\r\n";
    
    static Alarm messageAlarm = { 0u, 0u, false, AlarmType_ContinuousNotification };
    if (!messageAlarm.armed)
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
    if (alarm_hasElapsed(&messageAlarm))
    {
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
        uartCommon_write(ErrorMessage);
    }
}


/// Processes the case where the slave update initialization failed. The
/// function will intermittently transmit an ASCII error message over the host
/// UART bus.
void processHostUpdateFailed(void)
{
    static char const* ErrorMessage = "ERROR: slave update failed init!\r\n";
    
    static Alarm messageAlarm = { 0u, 0u, false, AlarmType_ContinuousNotification };
    if (!messageAlarm.armed)
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
    if (alarm_hasElapsed(&messageAlarm))
    {
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
        uartCommon_write(ErrorMessage);
    }
}


/// Processes the case where the host comm initialization failed. The function
/// will intermittently transmit an ASCII error message over the host UART bus.
void processHostCommFailed(void)
{
    static char const* ErrorMessage = "ERROR: heap memory low!\r\n";
    static char const* ErrorDetailFormat = "\t%d [%d|%d]\r\n";
    
    static Alarm messageAlarm = { 0u, 0u, false, AlarmType_ContinuousNotification };
    if (!messageAlarm.armed)
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
    if (alarm_hasElapsed(&messageAlarm))
    {
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
        uint16_t normalRequiredSize = uartCommon_getHeapWordRequirement(false);
        uint16_t updateRequiredSize = uartCommon_getHeapWordRequirement(true);
        uartCommon_write(ErrorMessage);
        char message[ERROR_MESSAGE_BUFFER_SIZE];
        smallSprintf(message, ErrorDetailFormat, HEAP_SIZE, normalRequiredSize, updateRequiredSize);
        uartCommon_write(message);
    }
}


/// Resets the bridge finite state machine (FSM) to the default state.
void reset(void)
{
    g_state = State_InitHostComm;
    resetHeap();
}


// === PUBLIC FUNCTIONS ========================================================

void bridgeFsm_init(void)
{
    reset();
}


void bridgeFsm_process(void)
{
    switch(g_state)
    {
        case State_InitHostComm:
        {
            if (processInitHostComm())
                g_state = State_InitSlaveReset;
            else
                g_state = State_HostCommFailed;
            break;
        }
        
        case State_InitSlaveReset:
        {
            if (processInitSlaveReset())
                g_state = State_CheckSlaveResetComplete;
            else
                g_state = State_InitSlaveTranslator;
            break;
        }
        
        case State_InitSlaveTranslator:
        {
            if (processInitSlaveTranslator())
                g_state = State_SlaveTranslator;
            else
                g_state = State_SlaveTranslatorFailed;
            break;
        }
        
        case State_InitSlaveUpdate:
        {
            processInitSlaveUpdate();
            g_state = State_SlaveUpdate;
            break;
        }
        
        case State_CheckSlaveResetComplete:
        {
            if (processSlaveResetComplete())
                g_state = State_InitSlaveTranslator;
            else if (!g_resetAlarm.armed)
                g_state = State_InitSlaveTranslator;
            break;
        }
        
        case State_SlaveTranslator:
        {
            processSlaveTranslator();
            break;
        }
        
        case State_SlaveUpdate:
        {
            processSlaveUpdate();
            break;
        }
        
        case State_SlaveTranslatorFailed:
        {
            processHostTranslatorFailed();
            // Do not transition out of this state.
            break;
        }
        
        case State_SlaveUpdateFailed:
        {
            processHostUpdateFailed();
            // Do not transition out of this state.
            break;
        }
        
        case State_HostCommFailed:
        {
            processHostCommFailed();
            // Do not transition out of this state.
            break;
        }
        
        default:
        {
            // Should not get here.
        }
    }
}


void bridgeFsm_requestTranslatorMode(void)
{
    g_modeChange.pending = false;
    g_modeChange.translatorPending = true;
}


void bridgeFsm_requestUpdateMode(void)
{
    g_modeChange.pending = false;
    g_modeChange.updatePending = true;
}


void bridgeFsm_requestReset(void)
{
    g_modeChange.pending = false;
    g_modeChange.resetPending = true;
}


/* [] END OF FILE */
