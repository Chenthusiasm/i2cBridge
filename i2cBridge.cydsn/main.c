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

#ifndef __cplusplus
    #include <stdbool.h>
#endif
#include <stdint.h>

#include "project.h"

#include "bridgeStateMachine.h"
#include "debug.h"
#include "hwSystemTime.h"
#include "i2cGen2.h"
#include "uartFrameProtocol.h"


// === DEFINES =================================================================

#define DEFAULT_SYSTICK_PERIOD_MS       (1u)


// === PRIVATE FUNCTIONS =======================================================

static void init(void)
{
    // Initialize the hardware resources.
    debug_init();
    hwSystemTime_init(DEFAULT_SYSTICK_PERIOD_MS);
    i2cGen2_init();
    uartFrameProtocol_init();
    
    // Initialize state machines and system controls.
    bridgeStateMachine_reset();
}


// === PUBLIC FUNCTIONS ========================================================

int main(void)
{
    CyGlobalIntEnable;

    init();
    
    for( ; ; )
    {
        bridgeStateMachine_process();
    }
}


/* [] END OF FILE */
