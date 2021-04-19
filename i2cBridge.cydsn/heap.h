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

#ifndef HEAP_H
    #define HEAP_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    /// The data type for a "word" in the heap data structures.
    typedef uint32_t heapWord_t;
    
    
    // === FUNCTIONS ===========================================================
    
    /// Calculates the word requirement to maintain word alignment given the
    /// size in bytes of the data structure.
    /// @param[in]  size    The size in bytes of the data structure to calculate
    ///                     the word requirement.
    uint16_t heap_calculateHeapWordRequirement(uint16_t size);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // HEAP_H


/* [] END OF FILE */
