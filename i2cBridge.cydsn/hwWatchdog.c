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


// === DEFINES =================================================================

/// The default watchdog timeout in milliseconds.
#define DEFAULT_TIMEOUT_MS              (2000u)


// === GLOBAL VARIABLES ========================================================

/// Flag indicating if the watchdog is running and therefore, must be fed in a
/// timely fashion to prevent a system reset.
static bool g_running = false;


/// The current watchdog timeout period.
static uint16_t g_timeoutMS = 0;


// === PRIVATE FUNCTIONS =======================================================


// === PUBLIC FUNCTIONS ========================================================

void hwWatchdog_init(uint16_t timeoutMS)
{
    if (g_running && (g_timeoutMS != 0))
    {
        hwWatchdog_feed();
        hwWatchdog_stop();
    }
    if (timeoutMS <= 0)
        timeoutMS = DEFAULT_TIMEOUT_MS;
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
