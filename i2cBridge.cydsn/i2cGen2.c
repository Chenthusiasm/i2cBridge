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

#include "i2cGen2.h"

#include "project.h"


// === GLOBALS =================================================================

static volatile bool g_rxPending = false;


// === ISR =====================================================================

CY_ISR(slaveISR)
{
    slaveIRQ_ClearPending();
    slaveIRQPin_ClearInterrupt();
        
    g_rxPending = true;
}


// === PUBLIC FUNCTIONS ========================================================

void i2cGen2_init(void)
{
    slaveI2C_Start();
    
    slaveIRQ_StartEx(slaveISR);
}


void i2cGen2_processRx(void)
{
    if (g_rxPending)
    {
        g_rxPending = false;
        
        // Read I2C data.
    }
}


void i2cGen2_read(uint8_t address, uint8_t* target, uint16_t length)
{
}


void i2cGen2_write(uint8_t address, uint8_t* source, uint16_t length)
{
    slaveI2C_I2CMasterClearStatus();
    slaveI2C_I2CMasterWriteBuf(address, source, length, slaveI2C_I2C_MODE_COMPLETE_XFER);
    
    if (slaveI2C_I2CMasterStatus() == 0)
        ;
}


/* [] END OF FILE */
