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
    #define STRINGIFY(a)                #a
    
    /// Helper macro to create a .h filename from a token.
    #define HEADER__(a)                 STRINGIFY(a.h)
    
    /// Create a .h filename from a token.
    #define HEADER(a)                   HEADER__(a)
    
    /// Helper macro used to expand preprocessor tokens before concatenation.
    #define PASTE__(a, b)               a ## b

    /// Concatenate two preprocessor tokens into a single resulting token.
    #define PASTE(a, b)                 PASTE__(a, b)
    
    /// Token past for Cypress components.
    #define COMPONENT(a, b)             PASTE__(a, b)
    
    /// Get the high byte from a 16-bit data type.
    #define HI_BYTE_16_BIT(x)           ((uint8_t)((x >>  8u) & 0xff))
    
    /// Get the low byte from a 16-bit data type.
    #define LO_BYTE_16_BIT(x)           ((uint8_t)((x >>  0u) & 0xff))
    
    /// Get byte[3] (highest byte) from a 32-bit data type.
    #define BYTE_3_32_BIT(x)            ((uint8_t)((x >> 24u) & 0xff))
    
    /// Get byte[2] from a 32-bit data type.
    #define BYTE_2_32_BIT(x)            ((uint8_t)((x >> 16u) & 0xff))
    
    /// Get byte[1] from a 32-bit data type.
    #define BYTE_1_32_BIT(x)            ((uint8_t)((x >>  8u) & 0xff))
    
    /// Get byte[0] (lowest byte) from a 32-bit data type.
    #define BYTE_0_32_BIT(x)            ((uint8_t)((x >>  0u) & 0xff))
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
    
#endif // UTILITY_H


/* [] END OF FILE */
