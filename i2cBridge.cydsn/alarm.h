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

#ifndef ALARM_H
    #define ALARM_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    /// Enumerations for the different types of alarms.
    typedef enum AlarmType
    {
        /// The alarm will continuously indicate that it has fired when queried
        /// by the user until it is rearmed.
        AlarmType_ContinuousNotification,
        
        /// The alarm will only indicate it has fired once; all subsequent
        /// queries checking if it has fired will indicate it hasn't until the
        /// user rearms the alarm.
        AlarmType_SingleNotification,
        
    } AlarmType;
    
    
    /// Definition of an Alarm.
    typedef struct Alarm
    {
        /// The system time when the alarm was armed in milliseconds.
        uint32_t    startTimeMS;
        
        /// The amount of time that needs to elapse before the alarm has "fired" in
        /// milliseconds.
        uint32_t    durationMS;
        
        /// Flag indicating whether the alarm is armed.
        bool        armed;
        
        // A oneShot timer reports expired only once.
        AlarmType   type;
        
    } Alarm;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Set and arm an alarm.
    /// @param  alarm       The alarm to set.
    /// @param  durationMS  Number of milliseconds after which this alarm should
    ///                     "fire".
    /// @param  type        The type of alarm.
    void alarm_arm(Alarm volatile* alarm, uint32_t durationMS, AlarmType type);
    
    /// Disarm an alarm. Also serves to initialize the alarm
    /// @param alarm   The alarm to disarm.
    void alarm_disarm(Alarm volatile* alarm);
    
    /// Add additional time to the duration to effectively snooze the alarm and
    /// expire at a later time.
    /// @param  alarm       The alarm to set.
    /// @param  additionalTimeMS    The number of milliseconds to add before the
    ///                             alarm "fires".
    void alarm_snooze(Alarm volatile* alarm, uint32_t additionalTimeMS);
    
    /// Check if an alarm has elapsed. Note, the alarm must also be armed in
    /// order for the result to indicate that the alarm has elapsed. In the case
    /// of a single notification alarm, when the check to elapse indicates the
    /// alarm has elapsed, the alarm will be disabled so subsequent checks will
    /// return false unless the alarm is rearmed.
    /// @param  alarm  The alarm to check if it has elapsed.
    /// @return Whether the alarm has elapsed (true) or not (false).
    /// @note   This function call will disarm a SingleNotification alarm if it
    ///         has elapsed.
    bool alarm_hasElapsed(Alarm volatile* alarm);
    
    
    #ifdef __cplusplus
    } // extern "C"
    #endif
    
#endif // ALARM_H

/* [] END OF FILE */
