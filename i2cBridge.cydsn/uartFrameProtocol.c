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

#include "uartFrameProtocol.h"

#include <stdio.h>
#include <string.h>

#include "project.h"

#include "hwSystemTime.h"
#include "queue.h"



// === TYPEDEFINES =============================================================

/// Defines the different states of the protocol state machine.
typedef enum RxState_
{
    /// Outside of a valid data frame, do not process this data.
    RxState_OutOfFrame,
    
    /// Inside a valid data frame, the data needs to be processed.
    RxState_InFrame,
    
    /// An escape character occurred, interpret the next byte as data instead of
    /// a control byte.
    RxState_EscapeCharacter,
    
} RxState;


/// Defines the different control bytes in the protocol which helps define a
/// data frame.
typedef enum ControlByte_
{
    /// Start of a valid data frame.
    ControlByte_StartFrame              = 0xaa,
    
    /// End of a valid data frame.
    ControlByte_EndFrame                = ControlByte_StartFrame,
    
    /// Escape character. Interpret the next byte as data instead of a control
    /// byte.
    ControlByte_Escape                  = 0x55,
    
} ControlByte;


typedef enum BridgeCommand_
{
    /// Host to bridge ACK over UART.
    BridgeCommand_ACK                   = 'A',
    
    /// I2C slave error.
    BridgeCommand_SlaveError            = 'E',
    
    /// Access the I2C slave address.
    BridgeCommand_SlaveAddress          = 'I',
    
    /// Bridge to I2C slave NACK over I2C.
    BridgeAddress_SlaveNACK             = 'N',
    
    /// Bridge I2C read from I2C slave.
    BridgeCommand_SlaveRead             = 'R',
    
    /// I2C communication timeout between bridge and I2C slave.
    BridgeCommand_SlaveTimeout          = 'T',
    
    /// Configures bridge to slave update mode.
    BridgeCommand_SlaveUpdate           = 'U',
    
    /// Bridge version information.
    BridgeCommand_Version               = 'V',
    
    /// Bridge I2C write to I2C slave.
    BridgeCommand_SlaveWrite            = 'W',
    
    /// Bridge to I2C slave ACK over I2C.
    BridgeCommand_SlaveACK              = 'a',
    
    /// Bridge reset.
    BridgeCommand_Reset                 = 'r',
    
} BridgeCommand;


/// Enumeration that defines the offsets of different types of bytes within the
/// UICO I2C protocol.
typedef enum PacketOffset_
{
    /// Offset in the data frame for the bridge command.
    PacketOffset_BridgeCommand          = 0u,
    
    /// Offset in the data frome for the data payload.
    PacketOffset_BridgeData             = 1u,
} PacketOffset;


/// Structure that defines a transmit queue element/member.
typedef struct TxQueueElement_
{
    /// Pointer to the buffer that contains the data to transmit.
    uint8_t* pBuffer;
    
    /// The number of bytes to transmit in pBuffer.
    uint16_t length;
} TxQueueElement;


// === DEFINES =================================================================

/// The amount of time between receipts of bytes before we automatically reset
/// the receive state machine.
#define RX_RESET_TIMEOUT_MS                 (2000u)

/// The size of the processed and pending receive buffer for UART transactions.
#define RX_BUFFER_SIZE                      (512u)


// === GLOBALS =================================================================

/// The current state in the protocol state machine for receive processing.
/// frame.
static volatile RxState g_rxState = RxState_OutOfFrame;

/// The post-processed receive buffer.  All post-processed received data is
/// stored in this buffer until a full data frame is processed.
static uint8_t g_processedRxBuffer[RX_BUFFER_SIZE];

/// Flag indicating if there's a valid packet in the processed receive buffer.
static volatile bool g_processedRxBufferValid = false;

/// The current index of data in the processed received data buffer.  We need
/// this index to persist because we may receive only partial packets; this
/// allows for continuous processing of received data.
static volatile uint16_t g_processedRxIndex = 0;

/// The last time data was received in milliseconds.
static volatile uint32_t g_lastRxTimeMS = 0;

/// Callback function that is invoked when data is received out of the frame
/// state machine.
static UartFrameProtocol_RxOutOfFrameCallback g_rxOutOfFrameCallback = NULL;

/// Callback function that is invoked when data is received but the receive
/// buffer is not large enough to store it so the data overflowed.
static UartFrameProtocol_RxFrameOverflowCallback g_rxFrameOverflowCallback = NULL;


// === EXTERNS =================================================================


// === PRIVATE FUNCTIONS =======================================================

/// Sets all the global variables pertaining to the processed receive buffer to
/// an initial state.
static void resetProcessedRxBufferParameters(void)
{
    g_processedRxBufferValid = false;
    g_processedRxIndex = 0;
    g_lastRxTimeMS = hwSystemTime_getCurrentMS();
}


/// This function checks if the byte is an end frame character.  This is mainly
/// needed when parsing/processing data that has been received over UART and is
/// in the UICO UART protocol format.
/// @param[in]  data    The data byte to check to see if it's an end frame
///                     character.
/// @return If the data byte is an end frame character (true) or not (false).
static bool isEndFrameCharacter(uint8_t data)
{
    return (data == ControlByte_EndFrame);
}


/// This function checks if the byte is an escape character.  This is mainly
/// needed when parsing/processing data that has been received over UART and is
/// in the UICO UART protocol format.
/// @param[in]  data    The data byte to check to see if it's an escape
///                     character.
/// @return If the data byte is an escape character (true) or not (false).
static bool isEscapeCharacter(uint8_t data)
{
    return (data == ControlByte_Escape);
}


/// This function checks if the byte requires an escape character.  This is
/// mainly needed when setting up data to be transmitted and we need to format
/// the data packet to fit the UICO UART protocol.
/// @param[in]  data    The data byte to check to see if it requires an escape
///                     character.
/// @return If the data byte requires an escape character (true) or not (false).
static bool requiresEscapeCharacter(uint8_t data)
{
    return ((data == ControlByte_StartFrame) ||
            (data == ControlByte_EndFrame) ||
            (data == ControlByte_Escape));
}


/// Handle any byte in the processed via the receive state machine that would
/// overflow because it doesn't fit in the receive buffer.
/// @param[in]  data    The data byte that overflowed (didn't fit in the receive
///                     buffer).
static void handleRxFrameOverflow(uint8_t data)
{
    if (g_rxFrameOverflowCallback != NULL)
        g_rxFrameOverflowCallback(data);
}


/// Processes the processed UART data (where the frame and escape characters are
/// removed).
/// @return If the UART command was successfully processed or not.
static bool processCompleteRxPacket(void)
{
    bool status = false;
    if ((g_processedRxBufferValid) && (g_processedRxIndex > PacketOffset_BridgeCommand))
    {
        uint8_t command = g_processedRxBuffer[PacketOffset_BridgeCommand];
        switch (command)
        {
            case BridgeCommand_ACK:
            {
                break;
            }
            
            case BridgeCommand_SlaveError:
            {
                break;
            }
            
            case BridgeCommand_SlaveAddress:
            {
                break;
            }
            
            case BridgeAddress_SlaveNACK:
            {
                break;
            }
            
            case BridgeCommand_SlaveRead:
            {
                break;
            }
            
            case BridgeCommand_SlaveTimeout:
            {
                break;
            }
            
            case BridgeCommand_SlaveUpdate:
            {
                break;
            }
            
            case BridgeCommand_Version:
            {
                break;
            }
            
            case BridgeCommand_SlaveWrite:
            {
                break;
            }
            
            case BridgeCommand_SlaveACK:
            {
                break;
            }
            
            case BridgeCommand_Reset:
            {
                break;
            }
            
            default:
            {
                break;
            }
        }
    }
    return status;
}


/// Processes the receive buffer with the intent of parsing out valid frames of
/// data.
/// @param[in]  source          The buffer to process and parse to find full
///                             valid frames of data.
/// @param[in]  sourceSize      The size of the source buffer in bytes.
/// @param[in]  sourceOffset    The offset to start parsing.
/// @return The number of bytes that were processed.
static uint16_t __attribute__((unused)) processReceivedData(uint8_t const source[], uint16_t sourceSize, uint16_t sourceOffset)
{
    // Track the number of bytes that was processed.
    uint32_t size = 0;
    
    // Iterate through all the received bytes via UART.
    bool exit = false;
    for (uint32_t i = sourceOffset; i < sourceSize; ++i)
    {
        if (exit)
            break;
        
        uint8_t data = source[i];
        ++size;
        
        switch (g_rxState)
        {
            case RxState_OutOfFrame:
            {
                if (data == ControlByte_StartFrame)
                {                        
                    resetProcessedRxBufferParameters();
                    g_rxState = RxState_InFrame;
                }
                else if (g_rxOutOfFrameCallback != NULL)
                    g_rxOutOfFrameCallback(data);
                break;
            }
            
            case RxState_InFrame:
            {
                if (isEscapeCharacter(data))
                    g_rxState = RxState_EscapeCharacter;
                else if (isEndFrameCharacter(data))
                {
                    if (g_rxState == RxState_InFrame)
                        g_processedRxBufferValid = true;
                    g_rxState = RxState_OutOfFrame;
                    exit = true;
                }
                else
                {
                    // Make sure the data will fit in the buffer before adding.
                    if (g_processedRxIndex < RX_BUFFER_SIZE)
                        g_processedRxBuffer[g_processedRxIndex++] = data;
                    else
                        handleRxFrameOverflow(data);
                }
                break;
            }
            
            case RxState_EscapeCharacter:
            {
                // Make sure the data will fit in the buffer before adding.
                if (g_processedRxIndex < RX_BUFFER_SIZE)
                {
                    g_processedRxBuffer[g_processedRxIndex++] = data;
                    g_rxState = RxState_InFrame;
                }
                else
                    handleRxFrameOverflow(data);
                break;
            }
            
            default:
            {
                // We should never get into this state, if we do, something
                // wrong happened.  Potentially do some error handling.
                
                // Transition to the default state: out of frame.
                g_rxState = RxState_OutOfFrame;
                break;
            }
        }
    }
    return size;
}


// === PUBLIC FUNCTIONS ========================================================

void uartFrameProtocol_init(void)
{
    // Initialize global variables.
    g_rxState = RxState_OutOfFrame;
    resetProcessedRxBufferParameters();
}


void uartFrameProtocol_registerRxOutOfFrameCallback(UartFrameProtocol_RxOutOfFrameCallback callback)
{
    if (callback != NULL)
        g_rxOutOfFrameCallback = callback;
}


void uartFrameProtocol_registerRxFrameOverflowCallback(UartFrameProtocol_RxFrameOverflowCallback callback)
{
    if (callback != NULL)
        g_rxFrameOverflowCallback = callback;
}


uint16_t uartFrameProtocol_processRxData(uint8_t const data[], uint16_t size)
{
    uint32_t receiveTimeMS = (uint32_t)hwSystemTime_getCurrentMS();
    
    // Check if we haven't received data in a while.
    if ((uint32_t)(receiveTimeMS - g_lastRxTimeMS) > RX_RESET_TIMEOUT_MS)
    {
        // Reset the receive state machine.
        uartFrameProtocol_init();
    }
    
    uint16_t processSize = 0;
    if ((data != NULL) && (size > 0))
    {
        uint16_t offset = 0;
        while (offset < size)
        {
            offset += processReceivedData(data, size, offset);
            
            // Check if there's a valid packet to process.
            if (g_processedRxBufferValid && (g_processedRxIndex > 0))
            {
                processCompleteRxPacket();
                resetProcessedRxBufferParameters();
            }
        }
    }
    
    // Update the last receive time.
    g_lastRxTimeMS = receiveTimeMS;
    
    return processSize;
}


uint16_t uartFrameProtocol_makeFormattedTxData(uint8_t const source[], uint16_t sourceSize, uint8_t target[], uint16_t targetSize)
{
    uint16_t t = 0;
    
    // Check if the input parameters are valid.
    if ((sourceSize > 0) && (source != NULL) && (targetSize > 0) && (target != NULL))
    {
        // Always put the start frame control byte in the beginning.
        target[t++] = ControlByte_StartFrame;
        
        // Iterate through the source buffer and copy it into transmit buffer.
        for (uint32_t s = 0; s < sourceSize; ++s)
        {
            if (t > targetSize)
            {
                t = 0;
                break;
            }
            uint8_t data = source[s];
            
            // Check to see if an escape character is needed.
            if (requiresEscapeCharacter(data))
            {
                target[t++] = ControlByte_Escape;
                if (t > targetSize)
                {
                    t = 0;
                    break;
                }
            }
            target[t++] = data;
        }
        
        if ((t > 0) && (t < targetSize))
            target[t++] = ControlByte_EndFrame;
    }
    return t;
}


/* [] END OF FILE */
