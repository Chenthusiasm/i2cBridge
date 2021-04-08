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
#define GET_TIME_MS()                   (hwSystemTime_getCurrentMS())


// === PUBLIC FUNCTIONS ========================================================

void alarm_arm(Alarm volatile* alarm, uint32_t durationMS, AlarmType type)
{
    if (alarm != NULL)
    {
        alarm->durationMS = durationMS;
        alarm->startTimeMS = GET_TIME_MS();
        alarm->type = type;
        alarm->armed = true;
    }
}


void alarm_disarm(Alarm volatile* alarm)
{
    if (alarm != NULL)
    {
        alarm->armed = false;
        alarm->durationMS = 0;
    }
}


void alarm_snooze(Alarm volatile* alarm, uint32_t additionalTimeMS)
{
    if (alarm != NULL)
    {
        if ((UINT32_MAX - additionalTimeMS) >= alarm->durationMS)
            alarm->durationMS += additionalTimeMS;
        else
            alarm->durationMS = UINT32_MAX;
    }
}


bool alarm_hasElapsed(Alarm volatile* alarm)
{
    // Flag indicating if the alarm has elapsed; initialized to false (has not
    // fired yet) and the subsequent code will determine if it has fired (true).
    bool elapsed = false;
    if (alarm != NULL)
    {
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
    }
    return elapsed;
}


/* [] END OF FILE */
