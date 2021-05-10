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
    
    /// Structure that holds the status of the bridge; in this case, the bridge
    /// state machine.
    typedef union BridgeStatus
    {
        /// 8-bit representation of the status. Used to get the bit mask created
        /// by the following anonymous struct of 1-bit flags.
        uint8_t mask;
        
        /// Anonymous struct of 1-bit flags indicating specific errors.
        struct
        {
            /// The error pertains to the translate/normal.
            bool translateError: 1;
            
            /// The error pertains to the update.
            bool updateError : 1;
            
            /// The action associated with the bridge finite state machine
            /// failed.
            bool actionFailed : 1;
            
            /// Invalid scratch buffer offset.
            bool invalidScratchOffset : 1;
            
            /// Invalid scratch buffer operation.
            bool invalidScratchBuffer : 1;
            
            /// Potential memory leak has occurred due to allocation and
            /// deallocation of heap units for the heap.
            bool memoryLeak : 1;
            
            /// Invalid state.
            bool invalidState : 1;
            
            /// Attempt(s) to reset the slave failed.
            bool slaveResetFailed : 1;
            
        };
        
    } BridgeStatus;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initialize the bridge state machine. This will also reset the bridge
    /// state machine.
    void bridgeFsm_init(void);

    /// Process tasks associated with the current state of the bridge state
    /// machine.
    void bridgeFsm_process(void);
    
    /// Prep the bridge to change to touch firmware translate mode.
    void bridgeFsm_requestTranslateMode(void);
    
    /// Prep the bridge to change to touch firmware update mode.
    void bridgeFsm_requestUpdateMode(void);
    
    /// Prep the bridge to prepare for a system reset.
    void bridgeFsm_requestReset(void);
    
    /// Checks the BridgeStatus and indicates if any error occurs.
    /// @param[in]  status  The BridgeStatus error flags.
    /// @return If an error occurred according to the BridgeStatus.
    bool bridgeFsm_errorOccurred(BridgeStatus const status);
    
    /// Accessor to get the BridgeStatus structure with no error flags set.
    /// @return The BridgeStatus structure with no error flags set.
    BridgeStatus bridgeFsm_getNoErrorBridgeStatus(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // BRIDGE_STATE_MACHINE_H


/* [] END OF FILE */
