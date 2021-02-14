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

typedef enum State_
{
    State_SlaveTranslator,
    State_SlaveUpdater,
} State;


// === DEFINES =================================================================

#define DEFAULT_STATE                   (State_SlaveTranslator)


// === GLOBAL VARIABLES ========================================================

static State g_state = DEFAULT_STATE;


// === PRIVATE FUNCTIONS =======================================================




// === PUBLIC FUNCTIONS ========================================================

void bridgeStateMachine_reset(void)
{
    g_state = DEFAULT_STATE;
}


void bridgeStateMachine_process(void)
{
    switch(g_state)
    {
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
