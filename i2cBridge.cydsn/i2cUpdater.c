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

#include "i2cUpdater.h"

#include <limits.h>

#include "alarm.h"
#include "error.h"
#include "project.h"
#include "queue.h"
#include "utility.h"


// === DEFINES =================================================================

/// Name of the slave I2C component.
#define SLAVE_I2C                       slaveI2c_

/// Name of the slave IRQ component.
#define SLAVE_IRQ                       slaveIrq_

/// Name of the slave IRQ pin component.
#define SLAVE_IRQ_PIN                   slaveIrqPin_

/// Size of the raw receive data buffer.
#define RX_BUFFER_SIZE                  (64u)

/// The max size of the transfer queue (the max number of queue elements).
#define XFER_QUEUE_MAX_SIZE             (8u)

/// The size of the data array that holds the queue element data in the transfer
/// queue.
#define XFER_QUEUE_DATA_SIZE            (600u)


// === TYPE DEFINES ============================================================

/// The master driver level function status type. The I2CMasterStatus function
/// returns a uint32_t, but the returned value only uses 16-bits at most.
typedef uint16_t mstatus_t;

/// The master driver level function return type. The I2CMasterWriteBuf,
/// I2CMasterReadBuf, and I2CMasterSendStop functions return a uint32_t. Only
/// the I2CMasterSendStop function requires the return type to be uint32_t due
/// to the timeout error. The other two functions' return type can fit in a
/// uint16_t. Therefore, the return type is defined as the larger uint32_t.
typedef uint32_t mreturn_t;


/// Pre-defined 7-bit addresses of slave devices on the I2C bus.
typedef enum SlaveAddress
{
    /// Default 7-bit I2C address of the main application for UICO generation 2
    /// duraTOUCH MCU's (dT1xx, dT2xx, and dT4xx).
    SlaveAddress_App                    = 0x48,
    
    /// Default 7-bit I2C address of the bootloader for UICO generation 2
    /// duraTOUCH MCU's (dT1xx, dT2xx, and dT4xx).
    SlaveAddress_Bootloader             = 0x58,
    
} SlaveAddress;


/// Definition of the host transfer queue data offsets.
typedef enum HostXferQueueDataOffset
{
    /// The I2C address.
    HostXferQueueDataOffset_Xfer        = 0u,
    
    /// The start of the data payload.
    HostXferQueueDataOffset_Data        = 1u,
    
} HostXferQueueDataOffset;


/// Definition of the duraTOUCH application I2C communication receive packet
/// offsets.
typedef enum AppRxPacketOffset
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
typedef enum AppTxPacketOffset
{
    /// Length byte offset.
    AppTxPacketOffset_BufferOffset      = 0u,
    
    /// Data payload offset.
    AppTxPacketOffset_Data              = 1u,
    
} AppTxPacketOffset;


/// Definition of the duraTOUCH application memory buffer offset used for
/// setting the app buffer offset in tramsit packets.
typedef enum AppBufferOffset
{
    /// Command buffer offset; used to write a command.
    AppBufferOffset_Command             = 0x00,
    
    /// Response buffer offset; used to read a command.
    AppBufferOffset_Response            = 0x20,
    
} AppBufferOffset;


/// Definition of all the available commands of the application.
typedef enum AppCommand
{
    /// Default touch scan mode: scan and only report changes in touch status.
    AppCommand_ScanAndReportChanges     = 0x01,
    
    /// Scan and report everything.
    AppCommand_ScanAndReportAll         = 0x02,
    
    /// Stop scanning.
    AppCommand_StopScan                 = 0x03,
    
    /// Accessor to modify a parameter.
    AppCommand_SetParameter             = 0x04,
    
    /// Accessor to read a parameter.
    AppCommand_GetParameter             = 0x05,
    
    /// Perform a touch rebaseline.
    AppCommand_Rebaseline               = 0x06,
    
    /// Get reset info or execute a reset.
    AppCommand_Reset                    = 0x07,
    
    /// Clear the tuning settings in flash.
    AppCommand_EraseFlash               = 0x08,
    
    /// Save current tuning settings from RAM to flash.
    AppCommand_WriteFlash               = 0x09,
    
    /// Echo test.
    AppCommand_Echo                     = 0x0a,
    
    /// Built-in self-test.
    AppCommand_Bist                     = 0x0b,
    
    /// Customer-specific command.
    AppCommand_CustomerSpecific         = 0x0c,
    
    /// Debug command.
    AppCommand_Debug                    = 0x0d,
} AppCommand;


/// States used by the communication finite state machine (FSM) to handle the
/// different steps in processing read/write transactions on the I2C bus.
typedef enum CommState
{
    /// The state machine free and is waiting for the next transaction.
    CommState_Waiting,
    
    /// Data to be read from the slave is pending.
    CommState_RxPending,
    
    /// App needs to be switched to the response buffer being active.
    CommState_RxSwitchToResponseBuffer,
    
    /// Read the length of the response.
    CommState_RxReadLength,
    
    /// Process the length after reading.
    CommState_RxProcessLength,
    
    /// Read the remaining extra data payload state.
    CommState_RxReadExtraData,
    
    /// Process the the extra data payload after reading.
    CommState_RxProcessExtraData,
    
    /// Clear the IRQ after a complete read.
    CommState_RxClearIrq,
    
    /// Check if the last receive transaction has completed.
    CommState_RxCheckComplete,
    
    /// Dequeue from the transfer queue and act (read or write) based on the
    /// type of transfer.
    CommState_XferDequeueAndAct,
    
    /// Check if the last read transfer queue transaction has completed.
    CommState_XferRxCheckComplete,
    
    /// Check if the last write transfer queue transaction has completed.
    CommState_XferTxCheckComplete,
    
} CommState;


/// I2C transfer direction: read or write.
typedef enum I2cDirection
{
    /// Write to the slave device.
    I2cDirection_Write                  = COMPONENT(SLAVE_I2C, I2C_WRITE_XFER_MODE),
    
    /// Read from the slave device.
    I2cDirection_Read                   = COMPONENT(SLAVE_I2C, I2C_READ_XFER_MODE),
    
} I2cDirection;


/// Contains information about the I2C tranfer including the slave address and
/// the direction (read or write).
typedef union I2cXfer
{
    /// 8-bit representation of
    uint8_t value;
    
    /// Anonymous struct that defines the different fields in the I2cXfer union.
    struct
    {
        /// The 7-bit slave address.
        uint8_t address : 7;
        
        /// The direction (read or write) of the transfer.
        I2cDirection direction : 1;
        
    };
    
} I2cXfer;


/// Contains the results of the processRxLength function.
typedef struct AppRxLengthResult
{
    /// Anonymous union that collects bit flags indicating specific problems
    /// in the length packet.
    union
    {
        /// General flag indicating the length packet was invalid.
        bool invalid;
        
        /// Anonymous struct to consolidate the different bit flags.
        struct
        {
            /// The command is invalid.
            bool invalidCommand : 1;
            
        #if !ENABLE_ALL_CHANGE_TO_RESPONSE
            
            /// An invalid command was read and probably caused because the app
            /// was not in the valid buffer (valid buffer = response buffer).
            bool invalidAppBuffer : 1;
            
        #endif // !ENABLE_ALL_CHANGE_TO_RESPONSE
            
            /// The length is invalid.
            bool invalidLength : 1;
            
            /// The parameters passed in to process are invalid.
            bool invalidParameters : 1;
        
        };
    };
    
    /// The size of of the data payload in bytes; if 0, then there was either an
    /// error or there is no additional data to receive.
    uint8_t dataPayloadSize;
    
} AppRxLengthResult;


/// Structure to hold variables associated with the communication finite state
/// machine (FSM).
typedef struct CommFsm
{
    /// Timeout alarm used to determine if the I2C receive transaction has timed
    /// out.
    Alarm timeoutAlarm;
    
    /// Number of bytes that need to be received (pending).
    uint16_t pendingRxSize;
    
    /// The slave IRQ was triggered so the slave indicating there's data to be
    /// read. This needs to be volatile because it's modified in an ISR.
    volatile bool rxPending;
    
    /// Flag indicating if another attempt at a read should be performed but
    /// switch to the response buffer first.
    bool rxSwitchToResponseBuffer;
    
    /// The current state.
    CommState state;
    
} CommFsm;


#if ENABLE_LOCKED_BUS_DETECTION
    
    /// Locked bus variables.
    typedef struct LockedBus
    {
        /// Alarm to determine after how long since a continuous bus busy error
        /// from the low level driver should the status flag be set indicating a
        /// locked bus.
        Alarm detectAlarm;
        
        /// Alarm to track when to attempt a recovery after a locked bus has
        /// been detected.
        Alarm recoverAlarm;
        
        /// Track the number of recovery attempts since the locked bus was
        /// detected. This can be used to determine when to trigger a system
        /// reset.
        uint8_t recoveryAttempts;
        
        /// Flag indicating if the bus is in the locked condition.
        bool locked;
        
    } LockedBus;

#endif // ENABLE_LOCKED_BUS_DETECTION


/// Data structure that defines memory used by the module in a similar fashion
/// to a heap. Globals are contained in this structure that are used when the
/// module is activated and then "deallocated" when the module is deactivated.
/// This allows the memory to be used by another module. Note that these modules
/// must be run in a mutual exclusive fashion (one or the other; no overlap).
typedef struct Heap
{
    /// Host transfer queue.
    Queue xferQueue;
    
    /// Array of transfer queue elements for the transfer queue.
    QueueElement xferQueueElements[XFER_QUEUE_MAX_SIZE];
    
    /// Array to hold the data of the elements in the transfer queue. Note that
    /// each transfer queue element has at least 2 bytes:
    /// [0]: I2cXfer (adress and direction)
    /// [1]:
    ///     read: number of bytes to read.
    ///     write: data payload...
    uint8_t xferQueueData[XFER_QUEUE_DATA_SIZE];
    
    /// The raw receive buffer.
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    
    /// The I2C address and direction associated with the transaction that is
    /// waiting to be enqueued into the transfer queue. This must be set prior
    /// to enqueueing data into the transfer queue.
    I2cXfer pendingQueueXfer;
    
} Heap;


/// Data structure that defines how the callsite variable used for error
/// tracking is organized.
///
/// [0:1]:  lowLevelCall
/// [2]:    isBusReady
/// [3]:    recoverFromLockedBus
/// [4:7]:  subCall
/// [8:15]: topCall
///
/// [0:7]:  subValue
/// [8:15]: topCall
///
/// [0:15]: value;
typedef union Callsite
{
    /// 16-bit value of the callsite as defined by callsite_t in the error
    /// module.
    callsite_t value;
    
    /// Anonymous struct that collects the 2 main parts of the callsite: the
    /// main site and sub value. 
    struct
    {
        /// Anonymous union that provides an alias for the sub value's members.
        union
        {
            /// Alias for the sub value's members.
            uint8_t subValue;
            
            /// Anonymous struct that collects the sub values invidual members.
            struct
            {
                /// Value of the lowest level private function call.
                uint8_t lowLevelCall : 2;
                
                /// Flag indicating if the isBusReady function was called.
                bool isBusReady : 1;
                
                /// Flag indicating if the recoverFromLockedBus function was
                /// called.
                bool recoverFromLockedBus : 1;
                
                /// Next level call below the top-level call. Used to help
                /// identify where specifically the error occurred.
                uint8_t subCall : 4;
                
            };
        };
        
        /// The public function that serves as the top-level invocation in the
        /// call chain within the module.
        uint8_t topCall : 8;
        
    };
} Callsite;



// === PUBLIC FUNCTIONS ========================================================



/* [] END OF FILE */
