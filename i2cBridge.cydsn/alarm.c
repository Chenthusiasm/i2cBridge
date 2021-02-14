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

#include "alarm.h"

#include "hwSystemTime.h"


// === DEFINES =================================================================

/// Alias to get the current system time. Redefine to match what works for the
/// system.
#define GET_TIME_MS()                   (hwSystemTime_getCurrentMS())


// === PUBLIC FUNCTIONS ========================================================

void __attribute__((nonnull)) alarm_arm(Alarm volatile* alarm, uint32_t durationMS, AlarmType type)
{
    alarm->durationMS = durationMS;
    alarm->startTimeMS = GET_TIME_MS();
    alarm->type = type;
    alarm->armed = true;
}


void __attribute__((nonnull)) alarm_disarm(Alarm volatile* alarm)
{
    alarm->armed = false;
    alarm->durationMS = 0;
}


bool __attribute__((nonnull)) alarm_hasElapsed(Alarm volatile* alarm)
{
    // Flag indicating if the alarm has elapsed; initialized to false (has not
    // fired yet) and the subsequent code will determine if it has fired (true).
    bool elapsed = false;
    if ((alarm->durationMS == 0 ) || (GET_TIME_MS() - alarm->startTimeMS >= alarm->durationMS))
    {
        if (alarm->type == AlarmType_SingleNotification)
        {
            if (alarm->armed)
            {
                elapsed = true;
                alarm->armed = false;
                alarm->durationMS = 0;
            }
        }
        else
        {
            elapsed = true;
            alarm->durationMS = 0;
        }
    }
    return elapsed;
}


/* [] END OF FILE */
