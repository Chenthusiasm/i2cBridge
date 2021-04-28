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
#include "i2c.h"
#include "i2cTouch.h"
#include "i2cUpdate.h"
#include "project.h"
#include "uart.h"
#include "uartTranslate.h"
#include "uartUpdate.h"
#include "utility.h"


// === DEFINES =================================================================

/// Name of the slave reset component.
#define SLAVE_RESET                     slaveReset_

/// The size of the heap for "dynamic" memory allocation in bytes.
#define HEAP_BYTE_SIZE                  (2500u)

/// The size of the heap for "dynamic" memory allocation. Note that the heap
/// contains data of type heapWord_t to keep word aligned; therefore, the byte
/// size is divided by the size of the heapWord_t. In the case of a 32-bit MCU,
/// the heapWord_t is a uint32_t (see heapWord_t).
#define HEAP_SIZE                       (HEAP_BYTE_SIZE / sizeof(heapWord_t))


// === TYPE DEFINES ============================================================

/// The different states of the state machine.
typedef enum State
{
    /// Initializes the host communication.
    State_InitHostComm,
    
    /// Initializes slave reset.
    State_InitSlaveReset,
    
    /// Initialize the slave translate mode.
    State_InitSlaveTranslate,
    
    /// Initialize the slave update mode.
    State_InitSlaveUpdate,
    
    /// Checks if the slave reset is complete.
    State_CheckSlaveResetComplete,
    
    /// Processes tasks related to the default I2C slave translate mode.
    State_SlaveTranslate,
    
    /// Processes tasks related to the I2C slave update mode.
    State_SlaveUpdate,
    
    /// The slave translate failed to initialize. Send a generic error message
    /// to the host.
    State_SlaveTranslateFailed,
    
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
        /// Change to touch firmware translate mode is pending.
        bool translatePending : 1;
        
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
static uint32_t const G_ErrorMessagePeriodMs = 5000u;

/// The default timeout in milliseconds for processing UART receives.
static uint32_t const G_UartProcessRxTimeoutMs = 2u;

/// The default timeout in milliseconds for processing UART transmits.
static uint32_t const G_UartProcessTxTimeoutMs = 3u;

/// The default timeout in milliseconds for processing I2C transactions.
static uint32_t const G_I2cProcessTimeoutMs = 5u;


// === GLOBAL VARIABLES ========================================================

/// The current state of the state machine.
static State g_state = State_InitHostComm;

/// Flags indicating if a mode change was requested and is pending action. Also
/// indicates the specific mode.
static ModeChange g_modeChange = { false };

/// An alarm used to indicate how long to hold the slave device in reset.
static Alarm g_resetAlarm;

/// An alarm used to indicate when to transmit error messages when the host comm
/// fails to initialize.
static Alarm g_errorMessageAlarm;

/// Heap data structure used for "dynamic" memory allocation.
static Heap g_heap;


// === PRIVATE FUNCTIONS =======================================================

/// Checks the SystemStatus and indicates if any error occurs.
/// @param[in]  status  The SystemStatus error flags.
/// @return If an error occurred according to the SystemStatus.
static bool errorOccurred(SystemStatus const status)
{
    return (status.mask > 0u);
}


/// Checks if the slave is resetting.
/// Note, the slave reset is configured as an open drain drives low GPIO. The
/// slave device's XRES line has an internal pull-up and is an active low reset.
/// @return If the slave is currently resetting (active low).
static bool isSlaveResetting(void)
{
    return (COMPONENT(SLAVE_RESET, Read)() <= 0);
}


/// Puts the slave in reset or takes it out of reset, depending on the reset
/// input parameter.
/// @param[in]  reset   Flag indicating if the slave should be put in reset or
///                     not.
static void resetSlave(bool reset)
{
    COMPONENT(SLAVE_RESET, Write)((reset) ? (0u) : (1u));
}


/// Get the remaining size in words of the heap, free for memory allocation.
/// @return The size, in words, that is free in the heap.
static uint16_t getFreeHeapWordSize(void)
{
    return (HEAP_SIZE - g_heap.freeOffset);
}


/// Processes any system errors that may have occurred.
static void processError(SystemStatus status)
{
    if (errorOccurred(status))
        error_tally(ErrorType_System);
}


/// Rearms/arms the error message alarm.
static void rearmErrorMessageAlarm(void)
{
    alarm_arm(&g_errorMessageAlarm, G_ErrorMessagePeriodMs, AlarmType_ContinuousNotification);
}


/// Resets the heap to the default value. Additionally deactivates/deallocates
/// the heaps used by the host and slave communications.
/// @return Status indicating if an error occured. See the definition of the
///         SystemStatus union.
static SystemStatus resetHeap(void)
{
    SystemStatus status = { false };
    
    // Deactivate/deallocate the communication sub systems if they're activated.
    uint16_t deactivationSize = 0;
    if (i2cTouch_isActivated())
        deactivationSize += i2cTouch_deactivate();
    if (i2cUpdate_isActivated())
        deactivationSize += i2cUpdate_deactivate();
    if (uartTranslate_isActivated())
        deactivationSize += uartTranslate_deactivate();
    if (uartUpdate_isActivated())
        deactivationSize += uartUpdate_deactivate();
    if (deactivationSize != g_heap.freeOffset)
        status.memoryLeak = true;
    g_heap.freeOffset = 0u;
    return status;
}


/// Initializes the host comm in translate mode.
/// @return Status indicating if an error occured. See the definition of the
///         SystemStatus union.
static SystemStatus initHostComm(void)
{
    SystemStatus status = { false };
    uint16_t size = uartTranslate_activate(&g_heap.data[g_heap.freeOffset], getFreeHeapWordSize());
    if (size > 0)
        g_heap.freeOffset += size;
    else
    {
        status.translateError = true;
        status.invalidScratchOffset = true;
        uint16_t requirement = uartTranslate_getHeapWordRequirement();
        if (getFreeHeapWordSize() < requirement)
            status.invalidScratchBuffer = true;
    }
    return status;
}


/// Initializes the host communication bus.
/// @return If host was initialized.
static bool processInitHostComm(void)
{
    resetHeap();
    SystemStatus status = initHostComm();
    processError(status);
    return !errorOccurred(status);
}


/// Initialize the slave reset.
/// @return If the slave reset was initialized successfully.
static bool processInitSlaveReset(void)
{
    SystemStatus status = { false };
    if (!isSlaveResetting())
    {
        static uint32_t const DefaultResetTimeoutMs = 100u;
        alarm_arm(&g_resetAlarm, DefaultResetTimeoutMs, AlarmType_ContinuousNotification);
        resetSlave(true);
    }
    else
        status.slaveResetFailed = true;
    return !errorOccurred(status);
}


/// Processes checking if the slave reset is complete.
/// @return If the slave reset completed.
static bool processSlaveResetComplete(void)
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
    else
    {
        uartTranslate_processRx(G_UartProcessRxTimeoutMs);
        uartTranslate_processTx(G_UartProcessTxTimeoutMs);
    }
    processError(status);
    return complete;
}


/// Processes all tasks associated with initializing the I2C slave translate.
/// @return If the initialization was successful.
static bool processInitSlaveTranslate(void)
{
    SystemStatus status = { false };
    if (!(uartTranslate_isActivated() && i2cTouch_isActivated()))
    {
        status = resetHeap();
        if (!errorOccurred(status))
        {
            status = initHostComm();
            if (!errorOccurred(status))
            {
                uint16_t size = i2cTouch_activate(&g_heap.data[g_heap.freeOffset], getFreeHeapWordSize());
                if (size > 0)
                    g_heap.freeOffset += size;
                else
                {
                    status.translateError = true;
                    status.invalidScratchOffset = true;
                    uint16_t requirement = i2cTouch_getHeapWordRequirement();
                    if (getFreeHeapWordSize() < requirement)
                        status.invalidScratchBuffer = true;
                    resetHeap();
                }
            }
        }
    }
    processError(status);
    return !errorOccurred(status);
}


/// Processes all tasks associated with the I2C slave translation.
/// @return If processed successfully.
static bool processSlaveTranslate(void)
{
    bool processed = false;
    if (true)
    {
        uartTranslate_processRx(G_UartProcessRxTimeoutMs);
        i2cTouch_process(G_I2cProcessTimeoutMs);
        uartTranslate_processTx(G_UartProcessTxTimeoutMs);
        processed = true;
    }
    return processed;
}


/// Processes all tasks associated with initializing the I2C slave update.
/// @return If the initialization was successful.
static bool processInitSlaveUpdate(void)
{
    SystemStatus status = { false };
    if (!(uartUpdate_isActivated() && i2cUpdate_isActivated()))
    {
        status = resetHeap();
        if (!errorOccurred(status))
        {
            uint16_t size = uartUpdate_activate(&g_heap.data[g_heap.freeOffset], getFreeHeapWordSize());
            if (size > 0)
            {
                g_heap.freeOffset += size;
                size = i2cUpdate_activate(&g_heap.data[g_heap.freeOffset], getFreeHeapWordSize());
                if (size > 0)
                    g_heap.freeOffset += size;
                else
                {
                    status.updateError = true;
                    status.invalidScratchOffset = true;
                    uint16_t requirement = i2cUpdate_getHeapWordRequirement();
                    if (getFreeHeapWordSize() < requirement)
                        status.invalidScratchBuffer = true;
                    resetHeap();
                }
            }
            else
            {
                status.updateError = true;
                status.invalidScratchOffset = true;
                uint16_t requirement = uartUpdate_getHeapWordRequirement();
                if (getFreeHeapWordSize() < requirement)
                    status.invalidScratchBuffer = true;
            }
        }
    }
    processError(status);
    return !errorOccurred(status);
}


/// Processes all tasks associated with the I2C slave update.
/// @return If processed successfully.
static bool processSlaveUpdate(void)
{
    bool processed = false;
    if (true)
    {
        uartUpdate_process();
        processed = true;
    }
    return processed;
}


/// Writes the heap size to UART.
static void writeHeapSize(void)
{
    uart_write("\theap = ");
    uart_writeHexUint16(HEAP_SIZE);
    uart_writeNewline();
}


/// Writes the translate heap requirement size to UART.
static void writeTranslateHeapRequirement(void)
{
    uart_write("\ttranslate = ");
    uart_writeHexUint16(uartTranslate_getHeapWordRequirement());
    uart_write(" + ");
    uart_writeHexUint16(i2cTouch_getHeapWordRequirement());
    uart_writeNewline();
}


/// Writes the update heap requirement size to UART.
static void writeUpdateHeapRequirement(void)
{
    uart_write("\tupdate = ");
    uart_writeHexUint16(uartUpdate_getHeapWordRequirement());
    uart_write(" + ");
    uart_writeHexUint16(i2cUpdate_getHeapWordRequirement());
    uart_writeNewline();
}


/// Processes the case where the slave translate initialization failed. The
/// function will intermittently transmit an ASCII error message over the host
/// UART bus.
static void processHostTranslateFailed(void)
{
    if (!g_errorMessageAlarm.armed)
        rearmErrorMessageAlarm();
    if (alarm_hasElapsed(&g_errorMessageAlarm))
    {
        rearmErrorMessageAlarm();
        uart_write("ERROR: slave translate failed init!\r\n");
        writeHeapSize();
        writeTranslateHeapRequirement();
    }
}


/// Processes the case where the slave update initialization failed. The
/// function will intermittently transmit an ASCII error message over the host
/// UART bus.
static void processHostUpdateFailed(void)
{
    if (!g_errorMessageAlarm.armed)
        rearmErrorMessageAlarm();
    if (alarm_hasElapsed(&g_errorMessageAlarm))
    {
        rearmErrorMessageAlarm();
        uart_write("ERROR: slave update failed init!\r\n");
        writeHeapSize();
        writeUpdateHeapRequirement();
    }
}


/// Processes the case where the host comm initialization failed. The function
/// will intermittently transmit an ASCII error message over the host UART bus.
static void processHostCommFailed(void)
{
    if (!g_errorMessageAlarm.armed)
        rearmErrorMessageAlarm();
    if (alarm_hasElapsed(&g_errorMessageAlarm))
    {
        rearmErrorMessageAlarm();
        uart_write("ERROR: heap memory low!\r\n");
        writeHeapSize();
        writeTranslateHeapRequirement();
        writeUpdateHeapRequirement();
    }
}


/// Resets the bridge finite state machine (FSM) to the default state.
static void reset(void)
{
    g_state = State_InitHostComm;
    resetHeap();
}


// === PUBLIC FUNCTIONS ========================================================

void bridgeFsm_init(void)
{
    reset();
    alarm_disarm(&g_resetAlarm);
    alarm_disarm(&g_errorMessageAlarm);
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
                g_state = State_InitSlaveTranslate;
            break;
        }
        
        case State_InitSlaveTranslate:
        {
            if (processInitSlaveTranslate())
                g_state = State_SlaveTranslate;
            else
                g_state = State_SlaveTranslateFailed;
            break;
        }
        
        case State_InitSlaveUpdate:
        {
            if (processInitSlaveUpdate())
                g_state = State_SlaveUpdate;
            else
                g_state = State_SlaveUpdateFailed;
            break;
        }
        
        case State_CheckSlaveResetComplete:
        {
            if (processSlaveResetComplete())
                g_state = State_InitSlaveTranslate;
            else if (!g_resetAlarm.armed)
                g_state = State_InitSlaveTranslate;
            break;
        }
        
        case State_SlaveTranslate:
        {
            processSlaveTranslate();
            break;
        }
        
        case State_SlaveUpdate:
        {
            processSlaveUpdate();
            break;
        }
        
        case State_SlaveTranslateFailed:
        {
            processHostTranslateFailed();
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


void bridgeFsm_requestTranslateMode(void)
{
    g_modeChange.pending = false;
    g_modeChange.translatePending = true;
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
