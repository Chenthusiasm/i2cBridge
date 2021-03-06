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

#include "hwWatchdog.h"

#include "debug.h"
#include "project.h"


// === DEFINES =================================================================

/// The default watchdog timeout in milliseconds.
#define DEFAULT_TIMEOUT_MS              (2000u)

#define WATCHDOG_INTERRUPT_NUMBER       (9u)


// === GLOBAL VARIABLES ========================================================

/// Flag indicating if the watchdog is running and therefore, must be fed in a
/// timely fashion to prevent a system reset.
static bool g_running = false;


/// The current watchdog timeout period.
static uint16_t g_timeoutMs = 0;


// === PRIVATE FUNCTIONS =======================================================


// === ISR =====================================================================

CY_ISR(watchdogIsr)
{
}


// === PUBLIC FUNCTIONS ========================================================

void hwWatchdog_init(uint16_t timeoutMs)
{
    if (g_timeoutMs == 0)
    {
        CyIntSetVector(WATCHDOG_INTERRUPT_NUMBER, watchdogIsr);
        CyIntEnable(WATCHDOG_INTERRUPT_NUMBER);
        
        CySysWdtWriteMode(CY_SYS_WDT_COUNTER0, CY_SYS_WDT_MODE_INT);
    }
    else if (g_running)
    {
        hwWatchdog_feed();
        hwWatchdog_stop();
    }
    if (timeoutMs <= 0)
        timeoutMs = DEFAULT_TIMEOUT_MS;
    g_timeoutMs = timeoutMs;
    hwWatchdog_start();
}


bool hwWatchdog_isRunning(void)
{
    return g_running;
}


void hwWatchdog_start(void)
{
    g_running = true;
}


void hwWatchdog_stop(void)
{
    g_running = false;
}


void hwWatchdog_feed(void)
{
}


/* [] END OF FILE */
