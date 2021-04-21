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

#include "i2cUpdate.h"

#include <limits.h>

#include "alarm.h"
#include "error.h"
#include "project.h"
#include "queue.h"
#include "utility.h"


// === DEFINES =================================================================

/// Size of the raw receive data buffer.
#define RX_BUFFER_SIZE                  (64u)

/// The max size of the transfer queue (the max number of queue elements).
#define XFER_QUEUE_MAX_SIZE             (8u)

/// The size of the data array that holds the queue element data in the transfer
/// queue.
#define XFER_QUEUE_DATA_SIZE            (600u)


// === TYPE DEFINES ============================================================



// === PUBLIC FUNCTIONS ========================================================



/* [] END OF FILE */
