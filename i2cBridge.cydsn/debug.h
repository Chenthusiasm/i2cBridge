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

#ifndef DEBUG_H
    #define DEBUG_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "project.h"
    
    #include "configProject.h"
    
    
    // === DEFINES =============================================================
    
    // !!IMPORTANT!! Do not modify the following definitions; they ensure that
    // the debug functions are properly compiled in and out of code based on
    // enabling/disabling the debug components in the *.cysch file. Either
    // disable the component in the *.cysch file or disable the debug option in
    // "configProject.h".
    
    #define ACTIVE_DEBUG_PIN_0          (false)
    #define ACTIVE_DEBUG_PIN_1          (false)
    #define ACTIVE_DEBUG_UART           (false)
    
    
    // === FUNCTIONS ===========================================================
    
    void debug_init(void);
    
    #if defined(CY_PINS_debugPin0_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_PIN_0
        #define ACTIVE_DEBUG_PIN_0      (true)
        
        void debug_setPin0(bool set);
        bool debug_isSetPin0(void);
        
    #else
        
        #define debug_setPin0(a)
        #define debug_isSetPin0()       (false)
        
    #endif
    
    #if defined(CY_PINS_debugPin1_H) && !defined(CY_SW_TX_UART_debugUART_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_PIN_1
        #define ACTIVE_DEBUG_PIN_1      (true)
        
        void debug_setPin1(bool set);
        bool debug_isSetPin1(void);
        
    #else
        
        #define debug_setPin1(a)
        #define debug_isSetPin1()       (false)
        
    #endif
    
    #if defined(CY_SW_TX_UART_debugUART_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_UART
        #define ACTIVE_DEBUG_UART       (true)
        
        void debug_uartWriteByte(uint8_t byte);
        void debug_uartWriteArray(uint8_t* pData, uint32_t length);
        void debug_uartPrint(char string[]);
        
    #else
        
        #define debug_uartWriteByte(a)
        #define debug_uartWriteArray(a, b)
        #define debug_uartPrint(a)
        
    #endif
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // DEBUG_H


/* [] END OF FILE */
