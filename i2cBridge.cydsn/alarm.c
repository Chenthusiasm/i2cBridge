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




// === PRIVATE_FUNCTIONS =======================================================




// === PUBLIC FUNCTIONS ========================================================

void __attribute__((nonnull)) alarm_arm(Alarm volatile* pAlarm, uint32_t durationMS, AlarmType type)
{
    pAlarm->durationMS = durationMS;
    pAlarm->startTimeMS = hwSystemTime_getCurrentMS();
    pAlarm->type = type;
    pAlarm->armed = true;
}


void __attribute__((nonnull)) alarm_disarm(Alarm volatile* pAlarm)
{
    pAlarm->armed = false;
    pAlarm->durationMS = 0;
}


bool __attribute__((nonnull)) alarm_hasElapsed(Alarm volatile* pAlarm)
{
    // Flag indicating if the alarm has elapsed; initialized to false (has not
    // fired yet) and the subsequent code will determine if it has fired (true).
    bool elapsed = false;
    
    if ((pAlarm->durationMS == 0 ) || (hwSystemTime_getCurrentMS() - pAlarm->startTimeMS >= pAlarm->durationMS))
    {
        if (pAlarm->type == AlarmType_SingleNotification)
        {
            if (pAlarm->armed)
            {
                elapsed = true;
                pAlarm->armed = false;
                pAlarm->durationMS = 0;
            }
        }
        else
        {
            elapsed = true;
            pAlarm->durationMS = 0;
        }
    }
    
    return elapsed;
}

/* [] END OF FILE */
