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

#include "debug.h"

#include "project.h"
#include "utility.h"


// === DEFINES =================================================================

/// Name of the debug UART component.
#define DEBUG_UART                      debugUart_

/// Name of the debug pin 0 component.
#define DEBUG_PIN_0                     debugPin0_

/// Name of the debug pin 1 component.
#define DEBUG_PIN_1                     debugPin1_


// === PUBLIC FUNCTIONS ========================================================

void debug_init(void)
{
    #if ACTIVE_DEBUG_UART
        COMPONENT(DEBUG_UART, Start)();
    #endif
    
    debug_setPin0(true);
    debug_setPin1(true);
}


#if ACTIVE_DEBUG_PIN_0
    
    void debug_setPin0(bool set)
    {
        COMPONENT(DEBUG_PIN_0, Write)((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin0(void)
    {
        return (COMPONENT(DEBUG_PIN_0, Read)() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_PIN_1
    
    void debug_setPin1(bool set)
    {
        COMPONENT(DEBUG_PIN_1, Write)((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin1(void)
    {
        return (COMPONENT(DEBUG_PIN_1, Read)() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_UART
    
    void debug_uartWriteByte(uint8_t byte)
    {
        COMPONENT(DEBUG_UART, PutChar)(byte);
    }
    
    
    void debug_uartWriteArray(uint8_t* pData, uint32_t length)
    {
        COMPONENT(DEBUG_UART, PutArray)(pData, length);
    }
    
    
    void debug_uartPrint(char string[])
    {
        COMPONENT(DEBUG_UART, PutString)(string);
    }
    
    
    void debug_uartPrintHexUint8(uint8_t data)
    {
        COMPONENT(DEBUG_UART, PutHexByte)(data);
    }
    
    
    void debug_uartPrintHexUint16(uint16_t data)
    {
        static uint8_t const Shift = 8u;
        debug_uartPrintHexUint8((uint8_t)(data >> Shift));
        debug_uartPrintHexUint8((uint8_t)(data));
    }
    
    
    void debug_uartPrintHexUint32(uint32_t data)
    {
        static uint8_t const Shift = 16u;
        debug_uartPrintHexUint16((uint16_t)(data >> Shift));
        debug_uartPrintHexUint16((uint16_t)(data));
    }
    
    
    void debug_uartPrintHexUint64(uint64_t data)
    {
        static uint8_t const Shift = 32u;
        debug_uartPrintHexUint32((uint32_t)(data >> Shift));
        debug_uartPrintHexUint32((uint32_t)(data));
    }
    
    
#endif


/* [] END OF FILE */
