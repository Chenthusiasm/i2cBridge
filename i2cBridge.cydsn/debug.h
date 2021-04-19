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

#ifndef DEBUG_H
    #define DEBUG_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    #include "config.h"
    #include "project.h"
    
    
    // === DEFINES =============================================================
    
    // !!IMPORTANT!! Do not modify the following definitions; they ensure that
    // the debug functions are properly compiled in and out of code based on
    // enabling/disabling the debug components in the *.cysch file. Either
    // disable the component in the *.cysch file or disable the debug option in
    // "config.h".
    
    #define ACTIVE_DEBUG_PIN_0          (false)
    #define ACTIVE_DEBUG_PIN_1          (false)
    #define ACTIVE_DEBUG_UART           (false)
    
    
    // === FUNCTIONS ===========================================================
    
    void debug_init(void);
    
    #if defined(CY_PINS_debugPin0_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_PIN_0
        #define ACTIVE_DEBUG_PIN_0      (true)
        
        void debug_setPin0(bool set);
        bool debug_isSetPin0(void);
        
    #else
        
        #define debug_setPin0(a)
        #define debug_isSetPin0()       (false)
        
    #endif
    
    #if defined(CY_PINS_debugPin1_H) && !defined(CY_SW_TX_UART_debugUart_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_PIN_1
        #define ACTIVE_DEBUG_PIN_1      (true)
        
        void debug_setPin1(bool set);
        bool debug_isSetPin1(void);
        
    #else
        
        #define debug_setPin1(a)
        #define debug_isSetPin1()       (false)
        
    #endif
    
    #if defined(CY_SW_TX_UART_debugUart_H)
        
        // !!IMPORTANT!! Do not modify the following macro definitions.
        #undef ACTIVE_DEBUG_UART
        #define ACTIVE_DEBUG_UART       (true)
        
        void debug_uartWriteByte(uint8_t byte);
        void debug_uartWriteArray(uint8_t* pData, uint32_t length);
        void debug_uartPrint(char string[]);
        void debug_uartPrintHexUint8(uint8_t data);
        void debug_uartPrintHexUint16(uint16_t data);
        void debug_uartPrintHexUint32(uint32_t data);
        void debug_uartPrintHexUint64(uint64_t data);
        
        /// Simplified implementation of printf. The format specifier is the %
        /// symbol. Floating point and scientific notation is not supported
        /// scientific notation.
        ///
        /// Note that the format specifier can have the following format:
        ///
        /// %[flags][width]specifier
        ///
        /// [flags]
        ///     -   Left-justify; right-justification is default. Associated
        ///         with [width]. Not applicable to the c, s, or p-specifiers.
        ///         Padding of spaces will occur to the right of the number;
        ///         zero-padding does not occur.
        ///     +   Forces the sign (+ or -) to be presented, even for positive
        ///         numbers. The default behavior is to only show the negative
        ///         symbol for negative numbers.
        ///     #   Precede b, o, x, or X-specifiers with their specific prefix
        ///         0b, 0, 0x, or 0X, respectively.
        ///     0   Left-pad the number with zeroes (0) instead of spaces
        ///         (default). Associated with [width]. Not applicable to the c,
        ///         s, or p-specifiers. If left-justify is enabled, this has no
        ///         impact.
        ///
        /// [width]
        ///     (number)    Minimum number of characters to printf. If the
        ///                 number to format is shorter than (number), the
        ///                 result is padded with spaces or zeroes (0),
        ///                 depending on the 0-flag. If the number to format is
        ///                 longer than (number), the result is not truncated.
        ///                 This is limited to (sizeof(uint32_t) * 8) + 2 or 34
        ///                 characters after applying the format specifier
        ///                 (including signs, prefixes, and padding). The c and
        ///                 s specifiers are not limited to 34 characters.
        ///
        /// specifier:
        ///     d   Signed decimal integer (base 10).
        ///     i   Signed decimal integer (base 10); identical to the
        ///         d-specifier.
        ///     b   Unsigned binary integer (base 2; not in the standard).
        ///     o   Unsigned octal integer (base 8).
        ///     x   Unsigned hexadecimal integer (base 16).
        ///     X   Unsigned hexadecimal integer (base 16). Uppercase.
        ///     c   Character.
        ///     s   String of characters. Must be NULL-terminated.
        ///     p   Pointer address in hexadecimal.
        ///     %   % character.
        ///
        /// @param[in]  format  The format string with % formatters for the
        ///                     variadic arguments.
        /// @param[in]  ...     The variadiac argument list representing the
        ///                     list of format specifiers with associated flags.
        void debug_printf(char const* format, ...);
        
    #else
        
        #define debug_uartWriteByte(a)
        #define debug_uartWriteArray(a, b)
        #define debug_uartPrint(a)
        #define debug_uartPrintHexUint8(a)
        #define debug_uartPrintHexUint16(a)
        #define debug_uartPrintHexUint32(a)
        #define debug_uartPrintHexUint64(a)
        #define debug_printf(a, ...)
        
    #endif
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // DEBUG_H


/* [] END OF FILE */
