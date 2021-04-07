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

#include "debug.h"

#include <stdarg.h>

#include "project.h"
#include "utility.h"


// === DEFINES =================================================================

/// Enable the optimized decimal divide and modulo; this doesn't use the
/// standard divide and modulo operators.
#define ENABLE_OPTIMIZED_DECIMAL_DIVIDE (true)

/// Enable the binary (base 2) formatter.
#define ENABLE_BINARY                   (true)

/// Enable the octal (base 8) formatter.
#define ENABLE_OCTAL                    (true)

/// Enable the hexadecimal (base 16) formatter.
#define ENABLE_HEX                      (true)

/// Name of the debug UART component.
#define DEBUG_UART                      debugUart_

/// Name of the debug pin 0 component.
#define DEBUG_PIN_0                     debugPin0_

/// Name of the debug pin 1 component.
#define DEBUG_PIN_1                     debugPin1_

/// Alias for the function to invoke which performs the character-specific
/// action in the printf function.
#define PUTCHAR(c)                      COMPONENT(DEBUG_UART, PutChar)(c)

#if ENABLE_BINARY

    /// The number of bits to right shift (divide) to perform the itoa
    /// conversion in binary.
    #define BINARY_SHIFT                (1u)

    /// The bit mask to get the remainder when performing division for the itoa
    /// conversion in binary.
    #define BINARY_MASK                 (1u << G_BinaryShift) - 1u)

#endif // ENABLE_BINARY

#if ENABLE_OCTAL
    
    /// The number of bits to right shift (divide) to perform the itoa
    /// conversion in binary.
    #define OCTAL_SHIFT                 (3u)
    
    /// The bit mask to get the remainder when performing division for the itoa
    /// conversion in binary.
    #define OCTAL_MASK                  (1u << G_OctalShift) - 1u)
    
#endif // ENABLE_OCTAL

// Do not encapsulate the following defines in #if ENABLE_HEX; these are needed
// for the pointer to string conversion.

/// The number of bits to right shift (divide) to perform the itoa
/// conversion in hexadecimal.
#define HEX_SHIFT                       (4u)

/// The bit mask to get the remainder when performing division for the itoa
/// conversion in hexadecimal.
#define HEX_MASK                        (1u << G_HexShift) - 1u)


// === TYPE DEFINES ============================================================

/// Definition for the default signed integer type for the itoa conversion.
typedef
#if (__SIZEOF_POINTER__ == 8u)
    int64_t
#elif (__SIZEOF_POINTER__ == 4u)
    int32_t
#elif (__SIZEOF_POINTER__ == 2u)
    int16_t
#elif (__SIZEOF_POINTER__ == 1u)
    int8_t
#else
    #error "simplePrintf.c: itoa_t definition unsupported pointer size."
#endif
    itoa_t;


/// Definition for the default unsigned integer type for the itoa conversion.
typedef
#if (__SIZEOF_POINTER__ == 8u)
    uint64_t
#elif (__SIZEOF_POINTER__ == 4u)
    uint32_t
#elif (__SIZEOF_POINTER__ == 2u)
    uint16_t
#elif (__SIZEOF_POINTER__ == 1u)
    uint8_t
#else
    #error "simplePrintf.c: uitoa_t definition unsupported pointer size."
#endif
    uitoa_t;

/// Base for to itoa-related functions.
typedef enum Base_
{
    /// Base 10.
    Base_Decimal,

    #if ENABLE_BINARY

    /// Base 2.
    Base_Binary,

    #endif // ENABLE_BINARY

    #if ENABLE_OCTAL

    /// Base 8.
    Base_Octal,

    #endif // ENABLE_OCTAL

    #if ENABLE_HEX

    /// Base 16 with lowercase alpha characters.
    Base_Hex,

    /// Base 16 with uppercase alpha characters.
    Base_UpperHex,

    #endif // ENABLE_HEX

} Base;


/// Results for itoa-related functions.
typedef struct ItoaResult_
{
    /// Pointer to the NULL-terminated string of the conversion.
    char* string;

    /// The length of the string (excluding the NULL-termination).
    uint8_t stringLength;

} ItoaResult;


/// Format specifier flags.
typedef union FormatFlags_
{
    /// Structure that defines the flag.
    struct
    {
        /// The minimum width of the formatted number.
        uint8_t width;

        /// Flag indicating if the formatted number is zero-padded; otherwise
        /// the default is to pad with space.
        bool zeroPad : 1;

        /// Flag indicating if the formatted number contains the prefix
        /// indicating the base: only for binary (0b), octal (0), and
        /// hexadecimal (0x or 0X).
        bool prefix : 1;

        /// Flag indicating if the formatted number is left-justified; otherwise
        /// the default is right-justified.
        bool left : 1;

        /// Flag indicating if the flag should be shown for both positive and
        /// negative integers. Only applies to decimal (base 10).
        bool sign : 1;

        /// Flag indicating if the formatted number is negative. This is needed
        /// to take advantage of the optimized divide and modulo function.
        bool negative : 1;

    };
    
    /// Value representation of the flags to ensure quick setting.
    uint16_t value;

} FormatFlags;


// === CONSTANTS ===============================================================

/// The divisor for decimal divison.
static uint32_t const G_DecimalDivisor = 10u;

/// Integer to character conversion table, lowercase (default).
static char const G_CharTable[] = "0123456789abcdef";

/// Integer to character conversion table, uppercase.
static char const G_UpperCharTable[] = "0123456789ABCDEF";


// === PRIVATE FUNCTIONS =======================================================

/// Simple function to find the length of a string.
/// @param[in]  string  The string to find the length of.
/// @return The length of the string in bytes.
static size_t stringLength(char const* string)
{
    char const* const start = string;
    while (*string != 0)
        ++string;
    return (string - start);
}


/// An optimized usigned integer divide by 10 that doesn't use the potentially
/// costly division or multiply operators in order to get the quotient and
/// remainder. Only supports up to 32-bit unsigned integers.
/// See http://homepage.divms.uiowa.edu/~jones/bcd/divide.html
/// @param[in]  d   The dividend (number to divide by 10).
/// @param[out] r   The remainder of the divide by 10.
/// @return The quotient as a result of dividing the dividend by 10.
static uint32_t divideBy10(uint32_t d, uint32_t* r)
{
    // Constants used to determine if we have a carry bit issue due to the
    // addition operation in the first line of the approximation.
    static uint32_t const DividendMaxLimit = 0xaaaaaaaa;
    static uint32_t const PostAddCarry = 0x80000000;

    uint32_t q;

    // Approximate calcuation of quotient in divide by 10.
    // q = d / 10 or (q + 1) = d / 10 for all possible uint32_t d.
    q = ((d >>  1) + d) >> 1;
    if (d > DividendMaxLimit)
        q += PostAddCarry;
    q = ((q >>  4) + q);
    q = ((q >>  8) + q);
    q = ((q >> 16) + q) >> 3;

    // Account for q = d / 10 or (q + 1) = d / 10 at this point. Also calculate
    // the remainder.
    uint32_t remainder = d - (((q << 2) + q) << 1);
    if (remainder >= G_DecimalDivisor)
    {
        *r = remainder - G_DecimalDivisor;
        ++q;
    }
    else
        *r = remainder;
    return q;
}


/// Performs a simplified unsigned integer to char string (itoa) conversion.
/// Base 10 (decimal), 2 (binary), 8 (octal), and 16 (hexadecimal) conversions
/// are permitted. Up to 32-bit integer support only; int64_t and uint64_t do
/// not function properly.
/// @param[in]  value   The integer to convert.
/// @param[in]  buffer  The buffer to store the converted string.
/// @param[in]  size    The size of the buffer; must account for null
///                     termination.
/// @param[in]  base    The base for the converted number. See Base enum.
/// @param[in]  flags   The format specifier flags.
/// @return The itoa result which contains the converted string and number of
///         characters in the converted string.
static ItoaResult simpleItoa(uint32_t value, char buffer[], uint8_t size, Base base, FormatFlags flags)
{
    static char const BinaryPrefix[] = "0b";
    static char const OctalPrefix[] = "0";
    static char const HexPrefix[] = "0x";
    static char const UpperHexPrefix[] = "0X";
    static char const PositivePrefix[] = "+";
    static char const NegativePrefix[] = "-";

    // Prepare variables.
    uint8_t prefixWidth = 0;
    char const* prefix = NULL;
    uint8_t i = size;
    buffer[--i] = 0;

    // Process the number.
    // 0 needs to be handled in a unique way, otherwise it won't get processed.
    if (value == 0)
    {
        buffer[--i] = '0';
        if (flags.sign && base == (Base_Decimal))
        {
            prefixWidth += sizeof(PositivePrefix) - 1u;
            prefix = PositivePrefix;
        }
    }
    else
    {
        uint32_t n = value;
        switch (base)
        {
            #if ENABLE_BINARY

            case Base_Binary:
            {
                while (n > 0)
                {
                    uint32_t r = n & G_BinaryMask;
                    n >>= G_BinaryShift;
                    buffer[--i] = G_CharTable[r];
                }
                if (flags.prefix)
                {
                    prefixWidth += sizeof(BinaryPrefix) - 1u;
                    prefix = BinaryPrefix;
                }
                break;
            }

            #endif // ENABLE_BINARY

            #if ENABLE_OCTAL

            case Base_Octal:
            {
                while (n > 0)
                {
                    uint32_t r = n & G_OctalMask;
                    n >>= G_OctalShift;
                    buffer[--i] = G_CharTable[r];
                }
                if (flags.prefix)
                {
                    prefixWidth += sizeof(OctalPrefix) - 1u;
                    prefix = OctalPrefix;
                }
                break;
            }

            #endif // ENABLE_OCTAL

            #if ENABLE_HEX

            case Base_Hex:
            {
                while (n > 0)
                {
                    uint32_t r = n & G_HexMask;
                    n >>= G_HexShift;
                    buffer[--i] = G_CharTable[r];
                }
                if (flags.prefix)
                {
                    prefixWidth += sizeof(HexPrefix) - 1u;
                    prefix = HexPrefix;
                }
                break;
            }

            case Base_UpperHex:
            {
                while (n > 0)
                {
                    uint32_t r = n & G_HexMask;
                    n >>= G_HexShift;
                    buffer[--i] = G_UpperCharTable[r];
                }
                if (flags.prefix)
                {
                    prefixWidth += sizeof(UpperHexPrefix) - 1u;
                    prefix = UpperHexPrefix;
                }
                break;
            }

            #endif // ENABLE_HEX

            default:
            {
                while (n > 0)
                {
                    uint32_t r;

                    #if ENABLE_OPTIMIZED_DECIMAL_DIVIDE

                    n = divideBy10(n, &r);

                    #else

                    r = n % G_DecimalDivisor;
                    n /= G_DecimalDivisor;

                    #endif // ENABLE_OPTIMIZED_DECIMAL_DIVIDE

                    buffer[--i] = G_UpperCharTable[r];
                }
                if (flags.negative)
                {
                    prefixWidth += sizeof(NegativePrefix) - 1u;
                    prefix = NegativePrefix;
                }
                else if (flags.sign)
                {
                    prefixWidth += sizeof(PositivePrefix) - 1u;
                    prefix = PositivePrefix;
                }
            }
        }
    }

    // Prepare variables.
    char* current = buffer;
    uint8_t numberWidth = size - (i + 1u);
    uint8_t padWidth = 0;
    if ((numberWidth + prefixWidth) < flags.width)
        padWidth = flags.width - (numberWidth + prefixWidth);
    ItoaResult result = { current, prefixWidth + padWidth + numberWidth };

    // Process left/right-justification, padding, and prefix.
    if (flags.left)
    {
        uint8_t j;
        for (j = 0; j < prefixWidth; ++j)
            current[j] = prefix[j];
        current += j;
        for (j = 0; j < numberWidth; ++j)
            current[j] = buffer[i + j];
        current += j;
        for (j = 0; j < padWidth; ++j)
            current[j] = ' ';
        current += j;
        current[j] = 0;
    }
    else
    {
        current = &buffer[i];
        uint8_t j;
        if ((padWidth > 0) && flags.zeroPad)
        {
            for (j = padWidth; j > 0; --j)
                *(--current) = '0';
        }
        for (j = prefixWidth; j > 0; --j)
            *(--current) = prefix[j - 1];
        if ((padWidth > 0) && !flags.zeroPad)
        {
            for (j = padWidth; j > 0; --j)
                *(--current) = ' ';
        }
        result.string = current;
    }
    return result;
}


/// Performs a simplified pointer to hexadecimal char string (itoa) conversion.
/// Supports up to 64-bit pointers.
/// @param[in]  value   The integer to convert.
/// @param[in]  buffer  The buffer to store the converted string.
/// @param[in]  size    The size of the buffer; must account for null
///                     termination.
/// @param[in]  base    The base for the converted number. See Base enum.
/// @param[in]  flags   The format specifier flags.
/// @return The itoa result which contains the converted string and number of
///         characters in the converted string.
static ItoaResult simplePtoa(void* pointer, char buffer[], uint8_t size, FormatFlags flags)
{
    char const Prefix[] = "0x";
    uint8_t const PrefixWidth = sizeof(Prefix) - 1u;
    uint8_t const PointerWidth = sizeof(void*) * 2u;

    // Prepare variables.
    uint8_t width = PointerWidth;
    width += (flags.prefix) ? (PrefixWidth) : (0u);
    uint8_t padWidth = 0;
    if (width < flags.width)
        padWidth = flags.width - width;
    if (width + padWidth >= size)
    {
        if (size > width)
            padWidth = (size - 1u) - width;
    }
    char* current = buffer;

    // Process standard justification padding if width exceeds minimum pointer
    // width.
    if (!flags.left && padWidth > 0)
    {
        for (uint8_t i = 0; i < padWidth; ++i)
            *current++ = ' ';
    }

    // Process the prefix if enabled.
    if (flags.prefix)
    {
        for (uint8_t i = 0; i < PrefixWidth; ++i)
            *current++ = Prefix[i];
    }

    // Process the pointer. This is done in reverse.
    uint8_t offset = PointerWidth;
    uitoa_t n = (uitoa_t)pointer;
    while (n > 0)
    {
        uint32_t r = n & G_HexMask;
        n >>= G_HexShift;
        current[offset - 1] = G_CharTable[r];
        --offset;
    }

    // Process 0-padding.
    while (offset > 0)
    {
        current[offset - 1] = '0';
        --offset;
    }
    current += PointerWidth;

    // Process left-justified padding if width exceeds minimum pointer width.
    if (flags.left && padWidth > 0)
    {
        for (uint8_t i = 0; i < padWidth; ++i)
            *current++ = ' ';
    }

    // Null-terminate.
    *current = 0;

    ItoaResult result = { (char*)buffer, width + padWidth };
    return result;
}


// === PUBLIC FUNCTIONS ========================================================

void debug_init(void)
{
    #if ACTIVE_DEBUG_UART
        COMPONENT(DEBUG_UART, Start)();
    #endif
    
    debug_setPin0(true);
    debug_setPin1(true);
}


#if ACTIVE_DEBUG_PIN_0
    
    void debug_setPin0(bool set)
    {
        COMPONENT(DEBUG_PIN_0, Write)((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin0(void)
    {
        return (COMPONENT(DEBUG_PIN_0, Read)() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_PIN_1
    
    void debug_setPin1(bool set)
    {
        COMPONENT(DEBUG_PIN_1, Write)((set) ? (1u) : (0u));
    }
    
    
    bool debug_isSetPin1(void)
    {
        return (COMPONENT(DEBUG_PIN_1, Read)() != 0);
    }
    
#endif // ACTIVE_DEBUG_PIN_0


#if ACTIVE_DEBUG_UART
    
    void debug_uartWriteByte(uint8_t byte)
    {
        COMPONENT(DEBUG_UART, PutChar)(byte);
    }
    
    
    void debug_uartWriteArray(uint8_t* pData, uint32_t length)
    {
        COMPONENT(DEBUG_UART, PutArray)(pData, length);
    }
    
    
    void debug_uartPrint(char string[])
    {
        COMPONENT(DEBUG_UART, PutString)(string);
    }
    
    
    void debug_uartPrintHexUint8(uint8_t data)
    {
        COMPONENT(DEBUG_UART, PutHexByte)(data);
    }
    
    
    void debug_uartPrintHexUint16(uint16_t data)
    {
        static uint8_t const Shift = 8u;
        debug_uartPrintHexUint8((uint8_t)(data >> Shift));
        debug_uartPrintHexUint8((uint8_t)(data));
    }
    
    
    void debug_uartPrintHexUint32(uint32_t data)
    {
        static uint8_t const Shift = 16u;
        debug_uartPrintHexUint16((uint16_t)(data >> Shift));
        debug_uartPrintHexUint16((uint16_t)(data));
    }
    
    
    void debug_uartPrintHexUint64(uint64_t data)
    {
        static uint8_t const Shift = 32u;
        debug_uartPrintHexUint32((uint32_t)(data >> Shift));
        debug_uartPrintHexUint32((uint32_t)(data));
    }
    
    int debug_printf(char const* format, ...)
    {
        #define MAX_WIDTH                   ((sizeof(uint32_t) * 8u) + 2u)
        #define BUFFER_SIZE                 (MAX_WIDTH + 1u)

        int n = 0;
        va_list args;
        va_start(args, format);
        
        bool formatSpecifier = false;
        FormatFlags flags;
        flags.value = 0;
        char c = *format++;
        char buffer[BUFFER_SIZE];
        ItoaResult result;
        while (c != 0)
        {
            if (formatSpecifier)
            {
                switch (c)
                {
                    case '#':
                    {
                        flags.prefix = true;
                        break;
                    }

                    case '%':
                    {
                        PUTCHAR(c);
                        ++n;
                        formatSpecifier = false;
                        break;
                    }

                    case '+':
                    {
                        flags.sign = true;
                        break;
                    }

                    case '-':
                    {
                        flags.left = true;
                        break;
                    }

                    case '0':
                    {
                        if (flags.width > 0)
                        {
                            flags.width *= G_DecimalDivisor;
                            if (flags.width > MAX_WIDTH)
                                flags.width = MAX_WIDTH;
                        }
                        else
                            flags.zeroPad = true;
                        break;
                    }

                    #if ENABLE_HEX

                    case 'X':
                    {
                        uint32_t arg = (uint32_t)va_arg(args, uint32_t);
                        result = simpleItoa(arg, buffer, BUFFER_SIZE, Base_UpperHex, flags);
                        break;
                    }

                    #endif // ENABLE_HEX

                    #if ENABLE_BINARY

                    case 'b':
                    {
                        uint32_t arg = (uint32_t)va_arg(args, uint32_t);
                        result = simpleItoa(arg, buffer, BUFFER_SIZE, Base_Binary, flags);
                        break;
                    }

                    #endif // ENABLE_BINARY

                    case 'c':
                    {
                        // Prepare variables. Assume we will always insert 1 char at
                        // minimum.
                        int m = 1;

                        // Process standard justified padding. Assume only 1 char.
                        if (!flags.left)
                        {
                            char padding = (flags.zeroPad) ? ('0') : (' ');
                            for ( ; m < flags.width; ++m)
                                PUTCHAR(padding);
                        }

                        // Process the character.
                        char arg = (char)va_arg(args, int);
                        PUTCHAR(arg);

                        // Process the left-justified padding. Zero-padding is not
                        // supported in left-justification.
                        if (flags.left)
                        {
                            for (m = 1; m < flags.width; ++m)
                                PUTCHAR(' ');
                        }
                        n += m;
                        formatSpecifier = false;
                        break;
                    }

                    case 'd':
                    case 'i':
                    {
                        int32_t arg = (int32_t)va_arg(args, int32_t);
                        uint32_t absoluteValue = arg;
                        if (arg < 0)
                        {
                            absoluteValue = -arg;
                            flags.negative = true;
                        }
                        result = simpleItoa(absoluteValue, buffer, BUFFER_SIZE, Base_Decimal, flags);
                        break;
                    }

                    #if ENABLE_OCTAL

                    case 'o':
                    {
                        uint32_t arg = (uint32_t)va_arg(args, uint32_t);
                        result = simpleItoa(arg, buffer, BUFFER_SIZE, Base_Octal, flags);
                        break;
                    }

                    #endif // ENABLE_OCTAL

                    case 'p':
                    {
                        void* arg = (void*)va_arg(args, void*);
                        result = simplePtoa(arg, buffer, BUFFER_SIZE, flags);
                        break;
                    }

                    case 's':
                    {
                        char const* arg = (char const*)va_arg(args, char const*);
                        
                        // Check if padding is needed with standard justification.
                        int m = 0;
                        if (!flags.left && (flags.width > 0))
                        {
                            m = (int)stringLength(arg);
                            if (m < flags.width)
                            {
                                char padding = (flags.zeroPad) ? ('0') : (' ');
                                for ( ; m < flags.width; ++m)
                                    PUTCHAR(padding);
                            }
                            while (*arg != 0)
                                PUTCHAR(*arg++);
                        }
                        else
                        {
                            while (*arg != 0)
                            {
                                PUTCHAR(*arg++);
                                ++m;
                            }
                        }

                        // Process left-justified padding.
                        if (flags.left)
                        {
                            for ( ; m < flags.width; ++m)
                                PUTCHAR(' ');
                        }
                        n += m;
                        formatSpecifier = false;
                        break;
                    }

                    case 'u':
                    {
                        flags.sign = false;
                        uint32_t arg = (uint32_t)va_arg(args, uint32_t);
                        result = simpleItoa(arg, buffer, BUFFER_SIZE, Base_Decimal, flags);
                        break;
                    }

                    #if ENABLE_HEX

                    case 'x':
                    {
                        uint32_t arg = (uint32_t)va_arg(args, uint32_t);
                        result = simpleItoa(arg, buffer, BUFFER_SIZE, Base_Hex, flags);
                        break;
                    }

                    #endif // ENABLE_HEX

                    default:
                    {
                        if ((c >= '1') && (c <= '9'))
                        {
                            flags.width *= G_DecimalDivisor;
                            flags.width += (c - '0');
                            if (flags.width > MAX_WIDTH)
                                flags.width = MAX_WIDTH;
                        }
                        else
                            formatSpecifier = false;
                    }
                }

                // Apply number formatting.
                if (formatSpecifier && (result.stringLength > 0))
                {
                    for (uint8_t i = 0; i < result.stringLength; ++i)
                        PUTCHAR(result.string[i]);
                    n += result.stringLength;
                    
                    formatSpecifier = false;
                }
            }
            else if (c == '%')
            {
                result.stringLength = 0;
                flags.value = 0;
                formatSpecifier = true;
            }
            else
            {
                PUTCHAR(c);
                ++n;
            }

            c = *format++;
        }

        #undef MAX_WIDTH
        #undef BUFFER_SIZE
        return n;
    }
    
    
#endif

void debug_test(void)
{
    debug_setPin1(false);
    for (uint32_t i = 0; i < 10000; ++i)
    {
        uint32_t r;
        uint32_t q = divideBy10(i, &r);
        if (r == 4)
            ;
    }
    debug_setPin1(true);
    
    CyDelay(100u);
    
    debug_setPin1(false);
    for (uint32_t i = 0; i < 10000; ++i)
    {
        static uint32_t const Divisor = 10u;
        uint32_t q = i / Divisor;
        uint32_t r = i % Divisor;
        if (r == 4)
            ;
    }
    debug_setPin1(true);
}


/* [] END OF FILE */
