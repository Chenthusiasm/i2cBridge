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

#ifndef UART_UPDATE_H
    #define UART_UPDATE_H

    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "heap.h"
    #include "queue.h"
    
    
    // === DEFINES =============================================================
    
    /// The max size of the receive queue (the max number of queue elements).
    #define UPDATE_RX_QUEUE_MAX_SIZE                        (4u)
    
    /// The size of the data array that holds the queue element data in the
    /// receive queue when in update mode.
    /// Note: in the previous implementation of the bridge, the Rx FIFO was
    /// allocated 2052 bytes; this should be larger than that.
    #define UPDATE_RX_QUEUE_DATA_SIZE                       (2100u)
    
    /// The max size of the transmit queue (the max number of queue elements).
    #define UPDATE_TX_QUEUE_MAX_SIZE                        (4u)

    /// The size of the data array that holds the queue element data in the
    /// transmit queue. This should be smaller than the receive queue data size
    /// to account for the change in the receive/transmit balance.
    #define UPDATE_TX_QUEUE_DATA_SIZE                       (100u)
    
    
    // === TYPE DEFINES ========================================================
    
    /// Enumeration that defines the offsets of the different slave update settings
    /// in the data payload of the Bridgecommand_SlaveUpdate command.
    typedef enum UpdateOffset
    {
        /// Flags associated with the update packet. See the UpdateFlags union.
        UpdateOffset_Flags                  = 0u,
        
        /// Offset for the file size. Note this is a big-endian 16-bit value.
        UpdateOffset_FileSize               = 1u,
        
        /// Offset for the chunk size.
        UpdateOffset_ChunkSize              = 3u,
        
        /// Offset for total number of chunks.
        UpdateOffset_NumberOfChunks         = 4u,
        
        /// Offset for the delay in milliseconds (currently not used).
        UpdateOffset_DelayMS                = 5u,
        
    } UpdateOffset;


    /// Enumeration that defines the offsets of the different fields in the
    /// update packet.
    typedef enum UpdateChunkOffset
    {
        /// Offset for the chunk size. Note this is a big-endian 16-bit value.
        UpdateChunkOffset_Size              = 0u,
        
        /// Offset for the data payload in the chunk.
        UpdateChunkOffset_Data              = 2u,
        
    } UpdateChunkOffset;


    /// Status/result of processing received data byte from the update packet.
    typedef enum RxUpdateByteStatus
    {
        /// Byte received fine. Continue receiving bytes for the subchunk.
        RxUpdateByteStatus_Success,
        
        /// Byte received fine. Subchunk complete, start a new subchunk.
        RxUpdateByteStatus_SubchunkComplete,
        
        /// Byte received fine. Chunk complete, start a new chunk.
        RxUpdateByteStatus_ChunkComplete,
        
        /// Byte received fine. File complete and all update data has been
        /// received, close the update.
        RxUpdateByteStatus_FileComplete,
        
        /// There was something wrong with the received byte; abort the update.
        RxUpdateByteStatus_Error,
        
    } RxUpdateByteStatus;

    /// Union that represents a bit-mask of different flags associated with the
    /// update.
    typedef union UpdateFlags
    {
        /// 8-bit value representation of the bit-mask.
        uint8_t value;
        
        struct
        {
            /// Bi-directional. Purpose unknown.
            bool initiate : 1;
            
            /// Only sent by the bridge. Indicates that the update was
            /// successful.
            bool success : 1;
            
            /// Not used.
            bool writeSuccess : 1;
            
            /// Only sent by the bridge. Indicates the bridge is ready for the
            /// next update chunk.
            bool readyForNextChunk : 1;
            
            /// Only sent by the host. Indicates that the update packet contains
            /// information regarding the update file.
            bool updateFileInfo : 1;
            
            /// Only sent by the bridge. Purpose unknown.
            bool test : 1;
            
            /// Only sent by the host. Associated with the *.txt file update.
            /// Untested and behavior is unknown.
            bool textStream : 1;
            
            /// Only sent by the bridge. Indicates there was a problem with the
            /// update.
            bool error : 1;
            
        };
        
    } UpdateFlags;


    /// Container of variables pertaining to the current the slave update chunk.
    typedef struct UpdateChunk
    {
        /// The total size in bytes of the chunk, update data only, in bytes.
        /// Used to help determine when the chunk has been completely sent.
        uint16_t totalSize;
        
        /// The current size of the chunk, update data only, in bytes.
        uint16_t size;
        
        /// The current size of the subchunk, update data only, in bytes.
        uint16_t subchunkSize;
        
    } UpdateChunk;


    /// Settings pertaining to the slave update. Note that these parameters are
    /// determined at runtime via the BridgeCommand_SlaveUpdate bridge command.
    /// File:       The entire data contents of the slave firmware update.
    /// Chunk:      Individual piece of the file that the host sends over the
    ///             host UART communication interface.
    /// Subchunk:   Individual piece of the chunk that the bridge sends over the
    ///             lsave I2C communication interface.
    ///
    /// F: file
    /// C: chunk
    /// S: subchunk
    ///
    /// FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    /// CCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCC
    /// SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS  SS
    typedef struct UpdateFile
    {
        /// Pointer to the current update chunk, containing info on the current
        /// chunk int he update process; if NULL, then the module is not in
        /// update mode.
        UpdateChunk* updateChunk;
        
        /// The total size of the update file (raw data only) in bytes (unused).
        uint16_t totalSize;
        
        /// The current size of the update file (raw data only) in bytes.
        uint16_t size;
        
        /// The size of a subchunk in bytes. The subchunk includes any header
        /// info along with actual update data.
        uint16_t subchunkSize;
        
        /// The toal number of chunks to expect in the entire update.
        uint8_t totalChunks;
        
        /// The current number of chunks in the entire update.
        uint8_t chunk;
        
        /// The delay in milliseconds (unused).
        uint8_t delayMS;
        
    } UpdateFile;
    
    
    /// Data extension for the Heap structure. Defines the data buffers when in
    /// update mode.
    typedef struct UpdateHeapData
    {
        /// Current status of the update chunk.
        UpdateChunk updateChunk;
        
        /// Array of decoded receive queue elements for the receive queue; these
        /// elements have been received but are pending processing.
        QueueElement decodedRxQueueElements[UPDATE_RX_QUEUE_MAX_SIZE];
        
        /// Array of transmit queue elements for the transmit queue.
        QueueElement txQueueElements[UPDATE_TX_QUEUE_MAX_SIZE];
        
        /// Array to hold the decoded data of elements in the receive queue.
        uint8_t decodedRxQueueData[UPDATE_RX_QUEUE_DATA_SIZE];
        
        /// Array to hold the data of the elements in the transmit queue.
        uint8_t txQueueData[UPDATE_TX_QUEUE_DATA_SIZE];
        
    } UpdateHeapData;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Initializes the communications interface.
    void uartUpdate_init(void);
    
    /// Accessor to get the number of heap words required for global variables.
    /// @return The number of heap words needed for global variables.
    uint16_t uartUpdate_getHeapWordRequirement(void);
    
    /// Activates the UART frame protocol module and sets up its globals.
    /// This must be invoked before using any processRx or processTx-like
    /// functions.
    /// @param[in]  memory          Memory buffer that is available for the
    ///                             module's globals. This is an array of 32-bit
    ///                             worlds to help preserve word alignment.
    /// @param[in]  size            The size (in 32-bit words) of the memory
    ///                             array.
    /// @return The number of 32-bit words the module used for its globals. If 0
    ///         Then there was an error and the module hasn't started.
    uint16_t uartUpdate_activate(heapWord_t memory[], uint16_t size);
    
    /// Deactivates the UART frame protocol module and effectively deallocates
    /// the global memory.
    /// @return The heap word size that was freed by deactivating. If 0, then
    ///         the module was probably not activated upon this function call so
    ///         there was nothing to deallocate.
    uint16_t uartUpdate_deactivate(void);
    
    /// Checks if the module is activated and the heap has been allocated for
    /// normal mode.
    /// @return If normal mode is activated.
    bool uartUpdate_isActivated(void);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // UART_UPDATE_H


/* [] END OF FILE */
