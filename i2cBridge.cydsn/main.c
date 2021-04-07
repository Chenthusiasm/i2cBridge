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
#include <stdio.h>
#include <string.h>

#include "bridgeStateMachine.h"
#include "debug.h"
#include "hwSystemTime.h"
#include "i2cGen2.h"
#include "project.h"
#include "uartFrameProtocol.h"


// === DEFINES =================================================================

/// The default system tick period.
#define DEFAULT_SYSTICK_PERIOD_MS       (1u)


// === PRIVATE FUNCTIONS =======================================================

/// Initialization of the hardware and system resources.
static void init(void)
{
    // Initialize the hardware resources.
    debug_init();
    debug_uartPrint("[i]debug\n");
    hwSystemTime_init(DEFAULT_SYSTICK_PERIOD_MS);
    debug_uartPrint("[i]sysTime\n");
    i2cGen2_init();
    debug_uartPrint("[i]i2c\n");
    uartFrameProtocol_init();
    debug_uartPrint("[i]uart\n");
    
    // Initialize state machines and system controls.
    bridgeStateMachine_init();
    debug_uartPrint("[i]bsm\n");
}


// === PUBLIC FUNCTIONS ========================================================

/// Main function.
int main(void)
{
    CyGlobalIntEnable;
    
    init();
    
    debug_test();
    
    debug_uartPrint("[LOOP]\n");
    for(int i = 0; ; ++i)
    {
        bridgeStateMachine_process();
    }
}


/* [] END OF FILE */
