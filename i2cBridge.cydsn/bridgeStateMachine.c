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

#include "bridgeStateMachine.h"

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
typedef enum State_
{
    /// Reset the slave.
    State_SlaveReset,
    
    /// Initialize the slave translator mode.
    State_InitSlaveTranslator,
    
    /// Initialize the slave updater mode.
    State_InitSlaveUpdater,
    
    /// Default I2C slave translator mode.
    State_SlaveTranslator,
    
    /// I2C slave update mode.
    State_SlaveUpdater,
} State;


// === GLOBAL VARIABLES ========================================================

/// The current state of the state machine.
static State g_state = State_SlaveReset;

/// Scratch buffer used for dynamic memory allocation by the comm modules.
/// @TODO: remove the temporary small scratch buffer when we're ready to use
/// the full scratch buffer.
#if 0
static uint32_t __attribute__((used)) g_scratchBuffer[SCRATCH_BUFFER_SIZE];
#else
static uint32_t __attribute__((used)) g_scratchBuffer[1];
#endif


// === PRIVATE FUNCTIONS =======================================================

/// Processes all tasks associated with resetting teh I2C slave.
void processSlaveReset(void)
{
    // @TODO: is there a way to check if the reset line is connected to ensure
    // we don't attempt to reset if the reset line is not connected.
    slaveReset_Write(0);
    CyDelay(100);
    slaveReset_Write(1);
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
    uint32_t const UARTProcessRxTimeoutMS = 2u;
    uint32_t const UARTProcessTxTimeoutMS = 3u;
    uint32_t const I2CProcessTxTimeoutMS = 5u;
    
    i2cGen2_processRx();
    uartFrameProtocol_processRx(UARTProcessRxTimeoutMS);
    i2cGen2_processRx();
    i2cGen2_processTxQueue(I2CProcessTxTimeoutMS, true);
    i2cGen2_processRx();
    uartFrameProtocol_processTx(UARTProcessTxTimeoutMS);
    i2cGen2_processRx();
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

void bridgeStateMachine_reset(void)
{
    g_state = State_SlaveReset;    
}


void bridgeStateMachine_init(void)
{
    bridgeStateMachine_reset();
}


void bridgeStateMachine_process(void)
{
    switch(g_state)
    {
        case State_SlaveReset:
        {
            processSlaveReset();
            g_state = State_InitSlaveTranslator;
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
