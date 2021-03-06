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


// === TYPE DEFINES ============================================================

/// The different states of the state machine.
typedef enum State_
{
    /// Default/reset state.
    State_Reset,
    
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
static State g_state = State_Reset;


// === PRIVATE FUNCTIONS =======================================================

/// Processes all tasks associated with initializing the I2C slave translator.
void processInitSlaveTranslator(void)
{
}


/// Processes all tasks associated with the I2C slave translator.
void processSlaveTranslator(void)
{
}


/// Processes all tasks associated with initializing the I2C slave updater.
void processInitSlaveUpdater(void)
{
}


/// Processes all tasks associated with the I2C slave updater.
void processSlaveUpdater(void)
{
}


// === PUBLIC FUNCTIONS ========================================================

void bridgeStateMachine_reset(void)
{
    g_state = State_Reset;
}


void bridgeStateMachine_process(void)
{
    switch(g_state)
    {
        case State_Reset:
        {
            g_state = State_InitSlaveTranslator;
            break;
        }
        
        case State_InitSlaveTranslator:
        {
            g_state = State_SlaveTranslator;
            break;
        }
        
        case State_InitSlaveUpdater:
        {
            g_state = State_SlaveUpdater;
            break;
        }
        
        case State_SlaveTranslator:
        {
            break;
        }
        
        case State_SlaveUpdater:
        {
            break;
        }
        
        default:
        {
            // Should not get here.
        }
    }
}


/* [] END OF FILE */
