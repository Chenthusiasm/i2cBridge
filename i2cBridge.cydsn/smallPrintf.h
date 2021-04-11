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

#ifndef SMALL_PRINTF_H
    #define SMALL_PRINTF_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    /// Definition of the put character callback function used by smallPrinf.
    /// The callback function is responsible for handling each individual
    /// character in the post-formatted string from the samllPrintf function.
    typedef void (*PutChar)(char);
    
    
    // === FUNCTIONS ===========================================================
        
    /// Simplified implementation of printf. The format specifier is the %
    /// symbol. Floating point and scientific notation is not supported
    /// scientific notation.
    ///
    /// Note that the format specifier can have the following format:
    ///
    /// %[flags][width]specifier
    ///
    /// [flags]
    ///     -   Left-justify; right-justification is default. Associated with
    ///         [width]. Not applicable to the c, s, or p-specifiers. Padding of
    ///         spaces will occur to the right of the number; zero-padding does
    ///         not occur.
    ///     +   Forces the sign (+ or -) to be presented, even for positive
    ///         numbers. The default behavior is to only show the negative
    ///         symbol for negative numbers.
    ///     #   Precede b, o, x, or X-specifiers with their specific prefix 0b,
    ///         0, 0x, or 0X, respectively.
    ///     0   Left-pad the number with zeroes (0) instead of spaces (default).
    ///         Associated with [width]. Not applicable to the c, s, or
    ///         p-specifiers. If left-justify is enabled, this has no impact.
    ///
    /// [width]
    ///     (number)    Minimum number of characters to printf. If the number
    ///                 to format is shorter than (number), the result is padded
    ///                 with spaces or zeroes (0), depending on the 0-flag. If
    ///                 the number to format is longer than (number), the result
    ///                 is not truncated. This is limited to 34 or
    ///                 (sizeof(uint32_t) * 8) + 2 characters after applying the
    ///                 format specifier (including signs, prefixes, and
    ///                 padding). The c and s specifiers are not limited to 34
    ///                 characters.
    ///
    /// specifier:
    ///     d   Signed decimal integer (base 10).
    ///     i   Signed decimal integer (base 10); identical to the d-specifier.
    ///     b   Unsigned binary integer (base 2; not in the standard).
    ///     o   Unsigned octal integer (base 8).
    ///     x   Unsigned hexadecimal integer (base 16).
    ///     X   Unsigned hexadecimal integer (base 16). Uppercase.
    ///     c   Character.
    ///     s   String of characters. Must be NULL-terminated.
    ///     p   Pointer address in hexadecimal.
    ///     %   % character.
    ///
    /// @param[in]  format  The format string with % formatters for the variadic
    ///                     arguments.
    /// @param[in]  ...     The variadiac argument list representing the list of
    ///                     format specifiers with associated flags.
    int smallPrintf(PutChar putChar, char const* format, ...);
    
    
    #ifdef __cplusplus
        } // extern "C"
    #endif
    
#endif // SMALL_PRINTF_H


/* [] END OF FILE */
