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


// === PUBLIC FUNCTIONS ========================================================

void debug_init(void)
{
    debugUART_Start();
    
    debug_setPin0(true);
    debug_setPin1(true);
}


#if ACTIVE_DEBUG_PIN_0
    
    void debug_setPin0(bool set)
    {
        debugPin0_Write((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin0(void)
    {
        return (debugPin0_Read() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_PIN_1
    
    void debug_setPin1(bool set)
    {
        debugPin1_Write((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin1(void)
    {
        return (debugPin1_Read() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_UART
    
    void debug_uartWriteByte(uint8_t byte)
    {
        debugUART_PutChar(byte);
    }
    
    
    void debug_uartWriteArray(uint8_t* pData, uint32_t length)
    {
        debugUART_PutArray(pData, length);
    }
    
    
    void debug_uartPrint(char string[])
    {
        debugUART_PutString(string);
    }
     
#endif


/* [] END OF FILE */
