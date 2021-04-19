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
#include "error.h"
#include "heap.h"
#include "i2cTouch.h"
#include "project.h"
#include "smallPrintf.h"
#include "uartFrameProtocol.h"


// === DEFINES =================================================================

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
    
    /// Initialize the slave updater mode.
    State_InitSlaveUpdater,
    
    /// Checks if the slave reset is complete.
    State_CheckSlaveResetComplete,
    
    /// Processes tasks related to the default I2C slave translator mode.
    State_SlaveTranslator,
    
    /// Processes tasks related to the I2C slave update mode.
    State_SlaveUpdater,
    
    /// The slave translator failed to initialize. Send a generic error message
    /// to the host.
    State_SlaveTranslatorFailed,
    
    /// The slave updater failed to initialize. Send a generic error message to
    /// the host.
    State_SlaveUpdaterFailed,
    
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
        
        /// Change to touch firmware updater mode is pending.
        bool updaterPending : 1;
        
        /// Reset request is pending.
        bool resetPending : 1;
        
    };
    
} ModeChange;


/// General status of the communication module: host UART or slave I2C.
typedef struct ModuleStatus
{
    /// The size of the communication module heap (in words);
    uint16_t heapWordSize;
    
    /// The offset in the heap for the communication module (in words).
    uint16_t heapWordOffset;
    
    /// Flag indicating if the communication module is active.
    bool active;
    
} ModuleStatus;


typedef struct HeapStatus
{
} HeapStatus;


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

/// The offset into the heap that indicates the start of free space.
static uint16_t g_freeHeapOffset = 0u;

/// Heap used for "dynamic" memory allocation by the comm modules.
static heapWord_t g_heap[HEAP_SIZE];


// === PRIVATE FUNCTIONS =======================================================

/// Get the remaining size in words of the heap, free for memory allocation.
/// @return The size, in words, that is free in the heap.
uint16_t getFreeHeapWordSize(void)
{
    return (HEAP_SIZE - g_freeHeapOffset);
}


/// Processes any system errors that may have occurred.
void processError(SystemStatus status)
{
    if (status.errorOccurred)
        error_tally(ErrorType_System);
}


/// Initializes the host comm in translator mode.
/// @return Status indicating if an error occured. See the definition of the
///         SystemStatus union.
SystemStatus initHostComm(void)
{
    static bool const EnableUpdater = false;
    
    SystemStatus status = { false };
    if (uartFrameProtocol_isUpdaterActivated())
    {
        // @TODO: call updater deactivate
        uartFrameProtocol_deactivate();
    }
    if (!uartFrameProtocol_isActivated())
    {
        uint16_t size = uartFrameProtocol_activate(
            &g_heap[g_freeHeapOffset],
            HEAP_SIZE - g_freeHeapOffset, EnableUpdater);
        if (size > 0)
            g_freeHeapOffset += size;
        else
        {
            status.invalidScratchOffset = true;
            uint16_t requirement = uartFrameProtocol_getHeapWordRequirement(EnableUpdater);
            if (getFreeHeapWordSize() < requirement)
                status.invalidScratchBuffer = true;
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
    bool processed = false;
    if (true)
    {
        processed = true;
        static uint32_t const DefaultResetTimeoutMS = 100u;
        alarm_arm(&g_resetAlarm, DefaultResetTimeoutMS, AlarmType_ContinuousNotification);
        slaveReset_Write(0);
    }
    return processed;
}


/// Processes all tasks associated with resetting the I2C slave.
/// @return If the slave reset completed.
bool processSlaveReset(void)
{
    bool processed = false;
    if (!g_resetAlarm.armed || alarm_hasElapsed(&g_resetAlarm))
    {
        slaveReset_Write(1);
        alarm_disarm(&g_resetAlarm);
        processed = true;
    }
    return processed;
}


/// Processes all tasks associated with initializing the I2C slave translator.
/// @return If the initialization was successful.
bool processInitSlaveTranslator(void)
{
    bool processed = false;
    SystemStatus status = initHostComm();
    if (!status.errorOccurred)
    {
        uint16_t size = i2cTouch_activate(&g_heap[g_freeHeapOffset], HEAP_SIZE - g_freeHeapOffset);
            
        if (size <= 0)
        {
            // Since one of the comm modules could not activate, deactivate both and
            uartFrameProtocol_deactivate();
            i2cTouch_deactivate();
            status.invalidScratchOffset = true;
            uint16_t requirement = i2cTouch_getHeapWordRequirement();
            if (getFreeHeapWordSize() < requirement)
                status.invalidScratchBuffer = true;
            status.translatorError = true;
        }
        else
            g_freeHeapOffset += size;
    }
    return processed;
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
        
        uartFrameProtocol_processRx(UartProcessRxTimeoutMS);
        i2cTouch_process(I2cProcessTimeoutMS);
        uartFrameProtocol_processTx(UartProcessTxTimeoutMS);
    }
    return processed;
}


/// Processes all tasks associated with initializing the I2C slave updater.
/// @return If the initialization was successful.
bool processInitSlaveUpdater(void)
{
    // @TODO: implement.
    return true;
}


/// Processes all tasks associated with the I2C slave updater.
/// @return If the updater processed successfully.
bool processSlaveUpdater(void)
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
        uartFrameProtocol_write(ErrorMessage);
    }
}


/// Processes the case where the slave updater initialization failed. The
/// function will intermittently transmit an ASCII error message over the host
/// UART bus.
void processHostUpdaterFailed(void)
{
    static char const* ErrorMessage = "ERROR: slave updater failed init!\r\n";
    
    static Alarm messageAlarm = { 0u, 0u, false, AlarmType_ContinuousNotification };
    if (!messageAlarm.armed)
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
    if (alarm_hasElapsed(&messageAlarm))
    {
        alarm_arm(&messageAlarm, G_ErrorMessagePeriodMS, AlarmType_ContinuousNotification);
        uartFrameProtocol_write(ErrorMessage);
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
        uint16_t normalRequiredSize = uartFrameProtocol_getHeapWordRequirement(false);
        uint16_t updaterRequiredSize = uartFrameProtocol_getHeapWordRequirement(true);
        uartFrameProtocol_write(ErrorMessage);
        char message[ERROR_MESSAGE_BUFFER_SIZE];
        smallSprintf(message, ErrorDetailFormat, HEAP_SIZE, normalRequiredSize, updaterRequiredSize);
        uartFrameProtocol_write(message);
    }
}


// === PUBLIC FUNCTIONS ========================================================

void bridgeFsm_reset(void)
{
    g_state = State_InitHostComm;    
}


void bridgeFsm_init(void)
{
    bridgeFsm_reset();
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
        
        case State_InitSlaveUpdater:
        {
            processInitSlaveUpdater();
            g_state = State_SlaveUpdater;
            break;
        }
        
        case State_CheckSlaveResetComplete:
        {
            if (processSlaveReset())
                g_state = State_InitSlaveTranslator;
            break;
        }
        
        case State_SlaveTranslator:
        {
            processSlaveTranslator();
            break;
        }
        
        case State_SlaveUpdater:
        {
            processSlaveUpdater();
            break;
        }
        
        case State_SlaveTranslatorFailed:
        {
            processHostTranslatorFailed();
            // Do not transition out of this state.
            break;
        }
        
        case State_SlaveUpdaterFailed:
        {
            processHostUpdaterFailed();
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


void bridgeFsm_requestUpdaterMode(void)
{
    g_modeChange.pending = false;
    g_modeChange.updaterPending = true;
}


void bridgeFsm_requestReset(void)
{
    g_modeChange.pending = false;
    g_modeChange.resetPending = true;
}


/* [] END OF FILE */
