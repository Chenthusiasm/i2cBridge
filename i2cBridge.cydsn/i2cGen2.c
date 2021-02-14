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

#include <stdio.h>

#include "project.h"
#include "queue.h"


// === TYPE DEFINES ============================================================

typedef enum AppPacketOffset_
{
    AppPacketOffset_Command             = 0u,
    AppPacketOffset_Length              = 1u,
    AppPacketOffset_Data                = 2u,
} AppPacketOffset;


typedef enum AppBufferOffset_
{
    AppBufferOffset_Command             = 0x00,
    AppBufferOffset_Response            = 0x20,
} AppBufferOffset;


// === DEFINES =================================================================

#define DATA_BUFFER_SIZE                (300u)

#define CLEAR_IRQ_PACKET_SIZE           (2u)


// === CONSTANTS ===============================================================

// Address of the slave I2C device.
static uint8_t const G_Address = 0x48;

static uint8_t const G_AppPacketLength = AppPacketOffset_Length + 1;

static uint8_t const G_InvalidAppPacketLength = 0xff;

/// The default amount of time to allow for a I2C stop condition to be sent
/// before timing out. If a time out occurs, the I2C module is reset.
static uint32_t const G_DefaultSendStopTimeout = 5u;

static uint8_t const G_ClearIRQPacket[CLEAR_IRQ_PACKET_SIZE] = { AppBufferOffset_Response, 0 };


// === GLOBALS =================================================================

static volatile bool g_rxPending = false;

static uint8_t g_buffer[DATA_BUFFER_SIZE];

static I2CGen2_RxCallback g_rxCallback = NULL;


// === ISR =====================================================================

CY_ISR(slaveISR)
{
    slaveIRQ_ClearPending();
    slaveIRQPin_ClearInterrupt();
        
    g_rxPending = true;
}


// === PRIVATE FUNCTIONS =======================================================

static bool __attribute__((unused)) isBusReady(void)
{
    return (slaveI2C_I2CMasterStatus() & slaveI2C_I2C_MSTAT_XFER_INP);
}


static bool isAppPacketLengthValid(uint8_t length)
{
    return (length < G_InvalidAppPacketLength);
}


static bool isIRQAsserted(void)
{
    return (slaveIRQPin_Read() == 0);
}


static void resetIRQ(void)
{
    static uint8_t clear[CLEAR_IRQ_PACKET_SIZE] = { AppBufferOffset_Response, 0 };
    slaveI2C_I2CMasterWriteBuf(G_Address, clear, CLEAR_IRQ_PACKET_SIZE, slaveI2C_I2C_MODE_COMPLETE_XFER);
}


// === PUBLIC FUNCTIONS ========================================================

void i2cGen2_init(void)
{
    slaveI2C_Start();
    
    slaveIRQ_StartEx(slaveISR);
}


void i2cGen2_registerRxCallback(I2CGen2_RxCallback pCallback)
{
    if (pCallback != NULL)
        g_rxCallback = pCallback;
}


bool i2cGen2_processRx(void)
{
    bool status = false;
    
    if (g_rxPending && isIRQAsserted())
    {
        if (slaveI2C_I2CMasterReadBuf(G_Address, g_buffer, G_AppPacketLength, slaveI2C_I2C_MODE_NO_STOP))
        {
            uint8_t length = g_buffer[AppPacketOffset_Length];
            if (isAppPacketLengthValid(length))
            {
                if (length > 0)
                    slaveI2C_I2CMasterReadBuf(G_Address, &g_buffer[AppPacketOffset_Data], length, slaveI2C_I2C_MODE_REPEAT_START);
                else
                    slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeout);
            }
            else
                slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeout);
        }
        
        resetIRQ();
           
        g_rxPending = false;
    }
    
    return status;
}


bool i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size)
{
    bool status = false;
    if ((data != NULL) && (size > 0))
        status = (slaveI2C_I2CMasterReadBuf(address, data, size, slaveI2C_I2C_MODE_COMPLETE_XFER) == slaveI2C_I2C_MSTR_NO_ERROR);
    return status;
}


bool i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size)
{
    bool status = false;
    if ((data != NULL) && (size > 0))
        status = (slaveI2C_I2CMasterWriteBuf(address, data, size, slaveI2C_I2C_MODE_COMPLETE_XFER) == slaveI2C_I2C_MSTR_NO_ERROR);
    return status;
}


bool i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size)
{
    static uint8_t const MinSize = 2u;
    static uint8_t const AddressOffset = 0u;
    static uint8_t const DataOffset = 1u;
    
    bool status = false;
    if ((data != NULL) && (size > MinSize))
    {
        size--;
        status = i2cGen2_write(data[AddressOffset], &data[DataOffset], size);
    }
    return status;
}


/* [] END OF FILE */
