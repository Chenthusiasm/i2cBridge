/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

// === DEPENDENCIES ============================================================

#include "heap.h"


// === PUBLIC FUNCTIONS ========================================================

uint16_t heap_calculateHeapWordRequirement(uint16_t size)
{
    uint16_t const Mask = sizeof(heapWord_t) - 1;
    
    if ((size & Mask) != 0)
        size = (size + sizeof(heapWord_t)) & ~Mask;
    return (size / sizeof(heapWord_t));
}


/* [] END OF FILE */
