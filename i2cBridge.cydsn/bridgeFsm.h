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
    
    
    // === TYPE DEFINES ========================================================
    
    /// Structure that holds the status of system; in this case, the bridge
    /// state machine.
    typedef union SystemStatus
    {
        /// General flag indicating an error occured; if false, no error
        /// occurred.
        bool errorOccurred;
        
        /// 8-bit representation of the status. Used to get the bit mask created
        /// by the following anonymous struct of 1-bit flags.
        uint8_t value;
        
        /// Anonymous struct of 1-bit flags indicating specific errors.
        struct
        {
            /// Invalid state.
            bool invalidState : 1;
            
            /// The error pertains to the translator.
            bool translatorError: 1;
            
            /// The error pertains to the updater.
            bool updaterError : 1;
            
            /// The action associated with the bridge finite state machine
            /// failed.
            bool actionFailed : 1;
            
            /// Invalid scratch buffer offset.
            bool invalidScratchOffset : 1;
            
            /// Invalid scratch buffer operation.
            bool invalidScratchBuffer : 1;
            
        };
        
    } SystemStatus;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Reset the bridge state machine.
    void bridgeFsm_reset(void);
    
    /// Initialize the bridge state machine. This will also reset the bridge
    /// state machine.
    void bridgeFsm_init(void);

    /// Process tasks associated with the current state of the bridge state
    /// machine.
    void bridgeFsm_process(void);
    
    /// Prep the bridge to change to touch firmware translator mode.
    void bridgeFsm_requestTranslatorMode(void);
    
    /// Prep the bridge to change to touch firmware updater mode.
    void bridgeFsm_requestUpdaterMode(void);
    
    /// Prep the bridge to change to touch firmware updater mode.
    void bridgeFsm_requestReset(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // BRIDGE_STATE_MACHINE_H


/* [] END OF FILE */
