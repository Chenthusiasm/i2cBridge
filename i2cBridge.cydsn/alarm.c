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

#include <limits.h>
#include <stdio.h>

#include "hwSystemTime.h"


// === DEFINES =================================================================

/// Alias to get the current system time. Redefine to match what works for the
/// system.
#define GET_TIME_MS()                   (hwSystemTime_getCurrentMs())


// === PUBLIC FUNCTIONS ========================================================

void alarm_arm(Alarm volatile* alarm, uint32_t durationMs, AlarmType type)
{
    if (alarm != NULL)
    {
        alarm->durationMs = durationMs;
        alarm->startTimeMs = GET_TIME_MS();
        alarm->type = type;
        alarm->armed = true;
    }
}


void alarm_disarm(Alarm volatile* alarm)
{
    if (alarm != NULL)
    {
        alarm->armed = false;
        alarm->durationMs = 0;
    }
}


void alarm_snooze(Alarm volatile* alarm, uint32_t additionalTimeMs)
{
    if ((alarm != NULL) && alarm->armed)
    {
        if ((UINT32_MAX - additionalTimeMs) >= alarm->durationMs)
            alarm->durationMs += additionalTimeMs;
        else
            alarm->durationMs = UINT32_MAX;
    }
}


bool alarm_hasElapsed(Alarm volatile* alarm)
{
    // Flag indicating if the alarm has elapsed; initialized to false (has not
    // fired yet) and the subsequent code will determine if it has fired (true).
    bool elapsed = false;
    if ((alarm != NULL) && alarm->armed)
    {
        if ((alarm->durationMs == 0 ) || (GET_TIME_MS() - alarm->startTimeMs >= alarm->durationMs))
        {
            if (alarm->type == AlarmType_SingleNotification)
                alarm->armed = false;
            alarm->durationMs = 0;
            elapsed = true;
        }
    }
    return elapsed;
}


/* [] END OF FILE */
