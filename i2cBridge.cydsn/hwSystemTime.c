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

#include "hwSystemTime.h"

#include "debug.h"
#include "project.h"


// === DEFINES =================================================================

/// The default initial current time in milliseconds.
#define DEFAULT_INITIAL_CURRENT_TIME_MS (0u)

/// The default period (rate at which the system timer interrupt fires) in
/// milliseconds.
#define DEFAULT_PERIOD_MS               (1u)


#define TICKS_MULTIPLIER_US             (8u)
#define TICKS_MULTIPLIER_MS             (TICKS_MULTIPLIER_US * 1000u)

#define ONE_MILLISECOND                 (CYDEV_BCLK__SYSCLK__KHZ)


// === GLOBALS =================================================================

static volatile uint32_t g_currentTimeMs = DEFAULT_INITIAL_CURRENT_TIME_MS;

static uint16_t g_periodMs = DEFAULT_PERIOD_MS;


// === ISR =====================================================================

CY_ISR(SysTickIsr)
{
    debug_setPin0(false);
    g_currentTimeMs += g_periodMs;
    debug_setPin0(true);
}


// === PUBLIC FUNCTIONS ========================================================

void hwSystemTime_init(uint16_t periodMs)
{
    // Reset the current time.
    g_currentTimeMs = DEFAULT_INITIAL_CURRENT_TIME_MS;
    
    g_periodMs = periodMs;
    
    // Configure the ISR for the system tick.
    CyIntSetSysVector((SysTick_IRQn + 16u), SysTickIsr);
    
    // Configure and enable the system tick.
    SysTick_Config(periodMs * ONE_MILLISECOND);
}


uint32_t hwSystemTime_getCurrentMs(void)
{
    return g_currentTimeMs;
}


/* [] END OF FILE */
