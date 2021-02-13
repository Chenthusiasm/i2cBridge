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
//#include "hwUart.h"



// === TYPEDEFINES =============================================================

/// Defines the different states of the protocol state machine.
typedef enum RxState_
{
    /// Outside of a valid data frame, do not process this data.
    RxState_OutOfFrame,
    
    /// Inside a valid data frame, the data needs to be processed.
    RxState_InFrame,
    
    /// Inside of a valid data frame but the Rx buffer has overflowed, so data
    /// will be dropped.
    RxState_InFrameOverflow,
    
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
    
    /// Bridge to I2C slave ACK over I2C.
    BridgeCommand_SlaveACK              = 'a',
    
    /// Bridge to I2C slave NACK over I2C.
    BridgeAddress_SlaveNACK             = 'N',
    
    /// I2C communication timeout between bridge and I2C slave.
    BridgeCommand_SlaveTimeout          = 'T',
    
    /// Bridge I2C write to I2C slave.
    BridgeCommand_SlaveWrite            = 'W',
    
    /// Bridge I2C read from I2C slave.
    BridgeCommand_SlaveRead             = 'R',
    
    /// Bridge reset.
    BridgeCommand_Reset                 = 'r',
    
    /// Bridge version information.
    BridgeCommand_Version               = 'V',
    
    /// Access the I2C slave address.
    BridgeCommand_SlaveAddress          = 'I',
    
    /// I2C slave error.
    BridgeCommand_SlaveError            = 'E',
    
    /// Configures bridge to slave update mode.
    BridgeCommand_SlaveUpdate           = 'U',
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
static UartOutOfFrameRxCallback g_outOfFrameRxCallback = NULL;


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


/// Processes the processed UART data (where the frame and escape characters are
/// removed).
/// @return If the UART command was successfully processed or not.
static bool __attribute__((unused)) processCompletePacket(void)
{
    bool status = false;
    
    // Make sure there's processed data in the receive buffer and there's at
    // least a command.
    if ((g_processedRxBufferValid) &&
        (g_processedRxIndex > PacketOffset_BridgeCommand))
    {
        uint8_t command = g_processedRxBuffer[PacketOffset_BridgeCommand];
        
        switch (command)
        {
            
            default:
            {
                // Send it along to the duraTOUCH library to process.
                //int result = duraTOUCH_receive((char*)g_processedRxBuffer, g_processedRxIndex);
                
                //status = (result == 0);
                
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
static uint16_t __attribute__((unused)) processReceivedData(uint8_t const* const pSource, uint16_t sourceSize, uint16_t sourceOffset)
{
    // Track the number of bytes that was processed.
    uint32_t size = 0;
    
    // Iterate through all the received bytes via UART.
    uint32_t sourceIndex = 0;
    bool exit = false;
    for (sourceIndex = sourceOffset; sourceIndex < sourceSize; ++sourceIndex)
    {
        // Check if we need to exit early.
        if (exit)
        {
            break;
        }
        
        uint8_t data = pSource[sourceIndex];
        ++size;
        
        switch (g_rxState)
        {
            case RxState_OutOfFrame:
            {
                if (data == ControlByte_StartFrame)
                {                        
                    // Reset the processed receive buffer parameters.
                    resetProcessedRxBufferParameters();
                    
                    // Transition to the next state.
                    g_rxState = RxState_InFrame;
                }
                else if (g_outOfFrameRxCallback != NULL)
                {
                    // Process the out of frame data.
                    g_outOfFrameRxCallback((char)data);
                }
                break;
            }
            
            case RxState_InFrame:
                // Fall through.
            
            case RxState_InFrameOverflow:
            {
                if (isEscapeCharacter(data))
                {
                    // Transition to the next state.
                    g_rxState = RxState_EscapeCharacter;
                }
                else if (isEndFrameCharacter(data))
                {
                    
                    if (g_rxState == RxState_InFrame)
                    {
                        g_processedRxBufferValid = true;
                    }
                    
                    // Transition to the next state.
                    g_rxState = RxState_OutOfFrame;
                    
                    exit = true;
                }
                else
                {
                    // Make sure the data will fit in the buffer.
                    if (g_processedRxIndex < RX_BUFFER_SIZE)
                    {
                        // Place the data into the processed received buffer and
                        // increment the processed receive buffer index.
                        g_processedRxBuffer[g_processedRxIndex++] = data;
                    }
                    else
                    {
                        // For now, drop the data and put us in an overflow
                        // state.
                        g_rxState = RxState_InFrameOverflow;
                    }
                }
                break;
            }
            
            case RxState_EscapeCharacter:
            {
                // Make sure the data will fit in the buffer.
                if (g_processedRxIndex < RX_BUFFER_SIZE)
                {
                    // Place the data into the processed received buffer and
                    // increment the processed receive buffer index.
                    g_processedRxBuffer[g_processedRxIndex++] = data;
                }
                else
                {
                    // For now, drop the data and put us in an overflow
                    // state.
                    g_rxState = RxState_InFrameOverflow;
                }
                
                // Always transition back to the in frame state after processing
                // the first byte after the escape character.
                g_rxState = RxState_InFrame;
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


void uartFrameProtocol_registerOutOfFrameRxCallback(UartOutOfFrameRxCallback pCallback)
{
    if (pCallback != NULL)
    {
        g_outOfFrameRxCallback = pCallback;
    }
}


uint16_t uartFrameProtocol_processRxData(uint8_t const* pBuffer, uint16_t bufferSize)
{
    uint32_t receiveTimeMS = (uint32_t)hwSystemTime_getCurrentMS();
    
    // Check if we haven't received data in a while.
    if ((uint32_t)(receiveTimeMS - g_lastRxTimeMS) > RX_RESET_TIMEOUT_MS)
    {
        // Reset the receive state machine.
        uartFrameProtocol_init();
    }
    
    uint16_t processSize = 0;
    if ((pBuffer != NULL) && (bufferSize > 0))
    {
        uint16_t offset = 0;
        while (offset < bufferSize)
        {
            offset += processReceivedData(pBuffer, bufferSize, offset);
            
            // Check if there's a valid packet to process.
            if (g_processedRxBufferValid && (g_processedRxIndex > 0))
            {
                // Process the packet.
                processCompletePacket();
            }
        }
    }
    
    // Update the last receive time.
    g_lastRxTimeMS = receiveTimeMS;
    
    return processSize;
}


uint16_t uartFrameProtocol_makeFormattedTxData(uint8_t const* pSource, uint16_t sourceLength, uint8_t* pTarget, uint16_t targetLength)
{
    uint16_t targetIndex = 0;
    
    // Check if the input parameters are valid.
    if ((sourceLength > 0) && (pSource != NULL) && (targetLength > 0) && (pTarget != NULL))
    {
        // Always put the start frame control byte in the beginning.
        pTarget[targetIndex++] = ControlByte_StartFrame;
        
        // Iterate through the source buffer and copy it into transmit buffer.
        uint16_t sourceIndex;
        for (sourceIndex = 0; sourceIndex < sourceLength; ++sourceIndex)
        {
            if (targetIndex > targetLength)
            {
                targetIndex = 0;
                break;
            }
            uint8_t data = pSource[sourceIndex];
            
            // Check to see if an escape character is needed.
            if (requiresEscapeCharacter(data))
            {
                pTarget[targetIndex++] = ControlByte_Escape;
                if (targetIndex > targetLength)
                {
                    targetIndex = 0;
                    break;
                }
            }
            pTarget[targetIndex++] = data;
        }
        
        if ((targetIndex > 0) && (targetIndex < targetLength))
        {
            // Always put in the end frame control byte at the end.
            pTarget[targetIndex++] = ControlByte_EndFrame;
        }
    }
    
    return targetIndex;
}





/* [] END OF FILE */
