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

#ifndef BRIDGE_STATE_MACHINE_H
    #define BRIDGE_STATE_MACHINE_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === FUNCTIONS ===========================================================
    
    /// Reset the bridge state machine.
    void bridgeStateMachine_reset(void);
    
    /// Initialize the bridge state machine. This will also reset the bridge
    /// state machine.
    void bridgeStateMachine_init(void);

    /// Process tasks associated with the current state of the bridge state
    /// machine.
    void bridgeStateMachine_process(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // BRIDGE_STATE_MACHINE_H


/* [] END OF FILE */
