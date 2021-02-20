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

/// Definition of the duraTOUCH application I2C communication receive packet
/// offsets.
typedef enum AppRxPacketOffset_
{
    /// Command byte offset.
    AppRxPacketOffset_Command           = 0u,
    
    /// Length byte offset.
    AppRxPacketOffset_Length            = 1u,
    
    /// Data payload offset.
    AppRxPacketOffset_Data              = 2u,
    
} AppRxPacketOffset;


/// Definition of the duraTOUCH application I2C communication transmit packet
/// offsets.
typedef enum AppTxPacketOffset_
{
    /// Length byte offset.
    AppTxPacketOffset_BufferOffset      = 0u,
    
    /// Data payload offset.
    AppTxPacketOffset_Data              = 1u,
    
} AppTxPacketOffset;


/// Definition of the duraTOUCH application memory buffer offset used for
/// setting the app buffer offset in tramsit packets.
typedef enum AppBufferOffset_
{
    /// Command buffer offset; used to write a command.
    AppBufferOffset_Command             = 0x00,
    
    /// Response buffer offset; used to read a command.
    AppBufferOffset_Response            = 0x20,
    
} AppBufferOffset;


// === DEFINES =================================================================

/// Size of the common data buffer.
#define DATA_BUFFER_SIZE                (300u)


// === CONSTANTS ===============================================================

/// Address of the slave I2C app.
static uint8_t const G_AppAddress = 0x48;

/// The number of bytes to read in order to find the number of bytes in the
/// receive data payload.
static uint8_t const G_AppRxPacketLengthSize = AppRxPacketOffset_Length + 1;

/// The value of the length byte which indicates the packet is invalid.
static uint8_t const G_InvalidRxAppPacketLength = 0xff;

/// The default amount of time to allow for a I2C stop condition to be sent
/// before timing out. If a time out occurs, the I2C module is reset. This is
/// in milliseconds.
static uint32_t const G_DefaultSendStopTimeoutMS = 5u;


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

/// Checks to see if the slave I2C bus is ready.
/// @return If the bus is ready for a new read/write transaction.
static bool __attribute__((unused)) isBusReady(void)
{
    return ((slaveI2C_I2CMasterStatus() & slaveI2C_I2C_MSTAT_XFER_INP) != 0);
}


/// Checks if the read packet contains a valid data payload length.
/// @param[in]  length  The data payload length.
/// @return If the length is valid for a read packet.
static bool isAppPacketLengthValid(uint8_t length)
{
    return (length < G_InvalidRxAppPacketLength);
}


/// Checks to see if the slave IRQ pin has been asserted, meaning there's data
/// ready to be read from the slave device.
/// @return If the slave IRQ pin is asserted.
static bool isIRQAsserted(void)
{
    return (slaveIRQPin_Read() == 0);
}


/// Create and sends the packet to the slave to instruct it to reset/clear the
/// IRQ line.
static void resetIRQ(void)
{
    static uint8_t clear[] = { AppBufferOffset_Response, 0 };
    slaveI2C_I2CMasterWriteBuf(G_AppAddress, clear, sizeof(clear), slaveI2C_I2C_MODE_COMPLETE_XFER);
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


int i2cGen2_processRx(void)
{
    int length = 0;
    if (g_rxPending && isIRQAsserted())
    {
        if (slaveI2C_I2CMasterReadBuf(G_AppAddress, g_buffer, G_AppRxPacketLengthSize, slaveI2C_I2C_MODE_NO_STOP))
        {
            uint8_t dataLength = g_buffer[AppRxPacketOffset_Length];
            if (isAppPacketLengthValid(dataLength))
            {
                if (dataLength > 0)
                    slaveI2C_I2CMasterReadBuf(G_AppAddress, &g_buffer[AppRxPacketOffset_Data], dataLength, slaveI2C_I2C_MODE_REPEAT_START);
                else
                    slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeoutMS);
            }
            else
                slaveI2C_I2CMasterSendStop(G_DefaultSendStopTimeoutMS);
        }
        resetIRQ();
        g_rxPending = false;
    }
    return length;
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
