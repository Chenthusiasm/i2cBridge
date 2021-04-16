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
#include "i2cGen2.h"
#include "project.h"
#include "uartFrameProtocol.h"


// === DEFINES =================================================================

/// The size of the scratch buffer. Note that the scratch buffer contains data
/// of type uint32_t to keek word aligned; therefore, we divide by the size of
/// a uint32_t.
#define SCRATCH_BUFFER_SIZE             (2500u / sizeof(uint32_t))


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
    
    /// Processes tasks related to the slave reset.
    State_SlaveReset,
    
    /// Processes tasks related to the default I2C slave translator mode.
    State_SlaveTranslator,
    
    /// Processes tasks related to the I2C slave update mode.
    State_SlaveUpdater,
    
} State;


// === GLOBAL VARIABLES ========================================================

/// The current state of the state machine.
static State g_state = State_InitHostComm;

/// Scratch buffer used for dynamic memory allocation by the comm modules.
/// @TODO: remove the temporary small scratch buffer when we're ready to use
/// the full scratch buffer.
#if 0
static uint32_t __attribute__((used)) g_scratchBuffer[SCRATCH_BUFFER_SIZE];
#else
static uint32_t __attribute__((used)) g_scratchBuffer[1];
#endif

/// The offset into the scratch buffer that indicates the start of free space
static uint16_t g_scratchOffset = 0u;

/// An alarm used to indicate how long to hold the slave device in reset.
static Alarm g_resetAlarm;


// === PRIVATE FUNCTIONS =======================================================

/// Processes any system errors that may have occurred.
void processError(SystemStatus status)
{
    if (status.errorOccurred)
    {
        error_tally(ErrorType_System);
    }
}


/// Initializes the host communication bus.
void processInitHostComm(void)
{
    SystemStatus status = { false };
    g_scratchOffset = uartFrameProtocol_activate(
        &g_scratchBuffer[g_scratchOffset],
        SCRATCH_BUFFER_SIZE - g_scratchOffset);
    if (g_scratchOffset <= 0)
    {
        status.invalidScratchOffset = true;
        uint16_t requirement = uartFrameProtocol_getMemoryRequirement();
        if (sizeof(g_scratchBuffer) < requirement)
            status.invalidScratchBuffer = true;
    }
    processError(status);
}


/// Initialize the slave reset.
/// @return The next state.
State processInitSlaveReset(void)
{
    static uint32_t const DefaultResetTimeoutMS = 100u;
    alarm_arm(&g_resetAlarm, DefaultResetTimeoutMS, AlarmType_ContinuousNotification);
    slaveReset_Write(0);
    return State_SlaveReset;
}


/// Processes all tasks associated with resetting the I2C slave.
/// @return The next state.
State processSlaveReset(void)
{
    State state = State_SlaveReset;
    if (!g_resetAlarm.armed || alarm_hasElapsed(&g_resetAlarm))
    {
        slaveReset_Write(1);
        alarm_disarm(&g_resetAlarm);
        state = State_InitSlaveTranslator;
    }
    return state;
}


/// Processes all tasks associated with initializing the I2C slave translator.
void processInitSlaveTranslator(void)
{
    uint16_t offset = uartFrameProtocol_activate(g_scratchBuffer, SCRATCH_BUFFER_SIZE);
    if (offset > 0)
        offset = i2cGen2_activate(&g_scratchBuffer[offset], SCRATCH_BUFFER_SIZE - offset);
        
    if (offset <= 0)
    {
        // Since one of the comm modules could not activate, deactivate both and
        uartFrameProtocol_deactivate();
        i2cGen2_deactivate();
        // @TODO: perform some additional error handling here.
    }
}


/// Processes all tasks associated with the I2C slave translator.
void processSlaveTranslator(void)
{
    uint32_t const UartProcessRxTimeoutMS = 2u;
    uint32_t const UartProcessTxTimeoutMS = 3u;
    uint32_t const I2cProcessRxTimeoutMS = 5u;
    uint32_t const I2cProcessTxTimeoutMS = 5u;
    
    i2cGen2_postProcessPreviousTransfer();
    uartFrameProtocol_processRx(UartProcessRxTimeoutMS);
    i2cGen2_processTxQueue(I2cProcessTxTimeoutMS, true);
    i2cGen2_processRx(I2cProcessRxTimeoutMS);
    uartFrameProtocol_processTx(UartProcessTxTimeoutMS);
}


/// Processes all tasks associated with initializing the I2C slave updater.
void processInitSlaveUpdater(void)
{
    // @TODO: implement.
}


/// Processes all tasks associated with the I2C slave updater.
void processSlaveUpdater(void)
{
    // @TODO: implement.
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
            g_state = State_InitSlaveReset;
            break;
        }
        
        case State_InitSlaveReset:
        {
            g_state = processInitSlaveReset();
            break;
        }
        
        case State_InitSlaveTranslator:
        {
            processInitSlaveTranslator();
            g_state = State_SlaveTranslator;
            break;
        }
        
        case State_InitSlaveUpdater:
        {
            processInitSlaveUpdater();
            g_state = State_SlaveUpdater;
            break;
        }
        
        case State_SlaveReset:
        {
            g_state = processSlaveReset();
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
        
        default:
        {
            // Should not get here.
        }
    }
}


/* [] END OF FILE */
