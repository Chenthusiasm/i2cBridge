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

#include "error.h"


// === DEFINES =================================================================


// === GLOBALS =================================================================

/// Indicates the current error mode. Default: legacy.
static ErrorMode g_mode = ErrorMode_Legacy;


// === PUBLIC FUNCTIONS ========================================================

ErrorMode error_getMode(void)
{
    return g_mode;
}

void error_setMode(ErrorMode mode)
{
    g_mode = mode;
}


/* [] END OF FILE */
