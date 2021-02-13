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

#ifndef UTILITY_H
    #define UTILITY_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    
    // === MACROS ==============================================================
    
    /// Helper macro for stringification.
    #define STRINGIFY(a)            #a
    
    /// Helper macro to create a .h filename from a token.
    #define HEADER__(a)             STRINGIFY(a.h)
    
    /// Create a .h filename from a token.
    #define HEADER(a)               HEADER__(a)
    
    /// Helper macro used to expand preprocessor tokens before concatenation.
    #define PASTE__(a, b)           a ## b

    /// Concatenate two preprocessor tokens into a single resulting token.
    #define PASTE(a, b)             PASTE__(a, b)
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // UTILITY_H


/* [] END OF FILE */
