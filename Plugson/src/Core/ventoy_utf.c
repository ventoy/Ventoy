/******************************************************************************
 * ventoy_utf.c  ---- ventoy utf
 * Copyright (c) 2022, Davipb https://github.com/Davipb/utf8-utf16-converter
 * Copyright (c) 2022, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <ventoy_define.h>
#include <ventoy_util.h>

typedef uint8_t utf8_t; // The type of a single UTF-8 character
typedef uint16_t utf16_t; // The type of a single UTF-16 character


// The type of a single Unicode codepoint
typedef uint32_t codepoint_t;

// The last codepoint of the Basic Multilingual Plane, which is the part of Unicode that
// UTF-16 can encode without surrogates
#define BMP_END 0xFFFF

// The highest valid Unicode codepoint
#define UNICODE_MAX 0x10FFFF

// The codepoint that is used to replace invalid encodings
#define INVALID_CODEPOINT 0xFFFD

// If a character, masked with GENERIC_SURROGATE_MASK, matches this value, it is a surrogate.
#define GENERIC_SURROGATE_VALUE 0xD800
// The mask to apply to a character before testing it against GENERIC_SURROGATE_VALUE
#define GENERIC_SURROGATE_MASK 0xF800

// If a character, masked with SURROGATE_MASK, matches this value, it is a high surrogate.
#define HIGH_SURROGATE_VALUE 0xD800
// If a character, masked with SURROGATE_MASK, matches this value, it is a low surrogate.
#define LOW_SURROGATE_VALUE 0xDC00
// The mask to apply to a character before testing it against HIGH_SURROGATE_VALUE or LOW_SURROGATE_VALUE
#define SURROGATE_MASK 0xFC00

// The value that is subtracted from a codepoint before encoding it in a surrogate pair
#define SURROGATE_CODEPOINT_OFFSET 0x10000
// A mask that can be applied to a surrogate to extract the codepoint value contained in it
#define SURROGATE_CODEPOINT_MASK 0x03FF
// The number of bits of SURROGATE_CODEPOINT_MASK
#define SURROGATE_CODEPOINT_BITS 10


// The highest codepoint that can be encoded with 1 byte in UTF-8
#define UTF8_1_MAX 0x7F
// The highest codepoint that can be encoded with 2 bytes in UTF-8
#define UTF8_2_MAX 0x7FF
// The highest codepoint that can be encoded with 3 bytes in UTF-8
#define UTF8_3_MAX 0xFFFF
// The highest codepoint that can be encoded with 4 bytes in UTF-8
#define UTF8_4_MAX 0x10FFFF

// If a character, masked with UTF8_CONTINUATION_MASK, matches this value, it is a UTF-8 continuation byte
#define UTF8_CONTINUATION_VALUE 0x80
// The mask to a apply to a character before testing it against UTF8_CONTINUATION_VALUE
#define UTF8_CONTINUATION_MASK 0xC0
// The number of bits of a codepoint that are contained in a UTF-8 continuation byte
#define UTF8_CONTINUATION_CODEPOINT_BITS 6

// Represents a UTF-8 bit pattern that can be set or verified
typedef struct
{
    // The mask that should be applied to the character before testing it
    utf8_t mask;
    // The value that the character should be tested against after applying the mask
    utf8_t value;
} utf8_pattern;

// The patterns for leading bytes of a UTF-8 codepoint encoding
// Each pattern represents the leading byte for a character encoded with N UTF-8 bytes,
// where N is the index + 1
static const utf8_pattern utf8_leading_bytes[] =
{
    { 0x80, 0x00 }, // 0xxxxxxx
    { 0xE0, 0xC0 }, // 110xxxxx
    { 0xF0, 0xE0 }, // 1110xxxx
    { 0xF8, 0xF0 }  // 11110xxx
};

// The number of elements in utf8_leading_bytes
#define UTF8_LEADING_BYTES_LEN 4


// Gets a codepoint from a UTF-16 string
// utf16: The UTF-16 string
// len: The length of the UTF-16 string, in UTF-16 characters
// index:
// A pointer to the current index on the string.
// When the function returns, this will be left at the index of the last character
// that composes the returned codepoint.
// For surrogate pairs, this means the index will be left at the low surrogate.
static codepoint_t decode_utf16(utf16_t const* utf16, size_t len, size_t* index)
{
    utf16_t high = utf16[*index];

    // BMP character
    if ((high & GENERIC_SURROGATE_MASK) != GENERIC_SURROGATE_VALUE)
        return high; 

    // Unmatched low surrogate, invalid
    if ((high & SURROGATE_MASK) != HIGH_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    // String ended with an unmatched high surrogate, invalid
    if (*index == len - 1)
        return INVALID_CODEPOINT;
    
    utf16_t low = utf16[*index + 1];

    // Unmatched high surrogate, invalid
    if ((low & SURROGATE_MASK) != LOW_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    // Two correctly matched surrogates, increase index to indicate we've consumed
    // two characters
    (*index)++;

    // The high bits of the codepoint are the value bits of the high surrogate
    // The low bits of the codepoint are the value bits of the low surrogate
    codepoint_t result = high & SURROGATE_CODEPOINT_MASK;
    result <<= SURROGATE_CODEPOINT_BITS;
    result |= low & SURROGATE_CODEPOINT_MASK;
    result += SURROGATE_CODEPOINT_OFFSET;
    
    // And if all else fails, it's valid
    return result;
}

// Calculates the number of UTF-8 characters it would take to encode a codepoint
// The codepoint won't be checked for validity, that should be done beforehand.
static int calculate_utf8_len(codepoint_t codepoint)
{
    // An array with the max values would be more elegant, but a bit too heavy
    // for this common function

    if (codepoint <= UTF8_1_MAX)
        return 1;

    if (codepoint <= UTF8_2_MAX)
        return 2;

    if (codepoint <= UTF8_3_MAX)
        return 3;

    return 4;
}

// Encodes a codepoint in a UTF-8 string.
// The codepoint won't be checked for validity, that should be done beforehand.
//
// codepoint: The codepoint to be encoded.
// utf8: The UTF-8 string
// len: The length of the UTF-8 string, in UTF-8 characters
// index: The first empty index on the string.
//
// return: The number of characters written to the string.
static size_t encode_utf8(codepoint_t codepoint, utf8_t* utf8, size_t len, size_t index)
{
    int size = calculate_utf8_len(codepoint);

    // Not enough space left on the string
    if (index + size > len)
        return 0;

    // Write the continuation bytes in reverse order first
    for (int cont_index = size - 1; cont_index > 0; cont_index--)
    {
        utf8_t cont = codepoint & ~UTF8_CONTINUATION_MASK;
        cont |= UTF8_CONTINUATION_VALUE;

        utf8[index + cont_index] = cont;
        codepoint >>= UTF8_CONTINUATION_CODEPOINT_BITS;
    }

    // Write the leading byte
    utf8_pattern pattern = utf8_leading_bytes[size - 1];

    utf8_t lead = codepoint & ~(pattern.mask);
    lead |= pattern.value;

    utf8[index] = lead;

    return size;
}

size_t utf16_to_utf8(utf16_t const* utf16, size_t utf16_len, utf8_t* utf8, size_t utf8_len)
{
    // The next codepoint that will be written in the UTF-8 string
    // or the size of the required buffer if utf8 is NULL
    size_t utf8_index = 0;

    for (size_t utf16_index = 0; utf16_index < utf16_len; utf16_index++)
    {
        codepoint_t codepoint = decode_utf16(utf16, utf16_len, &utf16_index);

        if (utf8 == NULL)
            utf8_index += calculate_utf8_len(codepoint);
        else
            utf8_index += encode_utf8(codepoint, utf8, utf8_len, utf8_index);
    }

    return utf8_index;
}

// Gets a codepoint from a UTF-8 string
// utf8: The UTF-8 string
// len: The length of the UTF-8 string, in UTF-8 characters
// index:
// A pointer to the current index on the string.
// When the function returns, this will be left at the index of the last character
// that composes the returned codepoint.
// For example, for a 3-byte codepoint, the index will be left at the third character.
static codepoint_t decode_utf8(utf8_t const* utf8, size_t len, size_t* index)
{
    utf8_t leading = utf8[*index];

    // The number of bytes that are used to encode the codepoint
    int encoding_len = 0;
    // The pattern of the leading byte
    utf8_pattern leading_pattern;
    // If the leading byte matches the current leading pattern
    int matches = 0;
    
    do
    {
        encoding_len++;
        leading_pattern = utf8_leading_bytes[encoding_len - 1];

        matches = ((leading & leading_pattern.mask) == leading_pattern.value);

    } while (!matches && encoding_len < UTF8_LEADING_BYTES_LEN);

    // Leading byte doesn't match any known pattern, consider it invalid
    if (!matches)
        return INVALID_CODEPOINT;

    codepoint_t codepoint = leading & ~leading_pattern.mask;

    for (int i = 0; i < encoding_len - 1; i++)
    {
        // String ended before all continuation bytes were found
        // Invalid encoding
        if (*index + 1 >= len)
            return INVALID_CODEPOINT;

        utf8_t continuation = utf8[*index + 1];

        // Number of continuation bytes not the same as advertised on the leading byte
        // Invalid encoding
        if ((continuation & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_VALUE)
            return INVALID_CODEPOINT;

        codepoint <<= UTF8_CONTINUATION_CODEPOINT_BITS;
        codepoint |= continuation & ~UTF8_CONTINUATION_MASK;

        (*index)++;
    }

    int proper_len = calculate_utf8_len(codepoint);

    // Overlong encoding: too many bytes were used to encode a short codepoint
    // Invalid encoding
    if (proper_len != encoding_len)
        return INVALID_CODEPOINT;

    // Surrogates are invalid Unicode codepoints, and should only be used in UTF-16
    // Invalid encoding
    if (codepoint < BMP_END && (codepoint & GENERIC_SURROGATE_MASK) == GENERIC_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    // UTF-8 can encode codepoints larger than the Unicode standard allows
    // Invalid encoding
    if (codepoint > UNICODE_MAX)
        return INVALID_CODEPOINT;

    return codepoint;
}

// Calculates the number of UTF-16 characters it would take to encode a codepoint
// The codepoint won't be checked for validity, that should be done beforehand.
static int calculate_utf16_len(codepoint_t codepoint)
{
    if (codepoint <= BMP_END)
        return 1;

    return 2;
}

// Encodes a codepoint in a UTF-16 string.
// The codepoint won't be checked for validity, that should be done beforehand.
//
// codepoint: The codepoint to be encoded.
// utf16: The UTF-16 string
// len: The length of the UTF-16 string, in UTF-16 characters
// index: The first empty index on the string.
//
// return: The number of characters written to the string.
static size_t encode_utf16(codepoint_t codepoint, utf16_t* utf16, size_t len, size_t index)
{
    // Not enough space on the string
    if (index >= len)
        return 0;

    if (codepoint <= BMP_END)
    {
        utf16[index] = codepoint;
        return 1;
    }

    // Not enough space on the string for two surrogates
    if (index + 1 >= len)
        return 0;

    codepoint -= SURROGATE_CODEPOINT_OFFSET;

    utf16_t low = LOW_SURROGATE_VALUE;
    low |= codepoint & SURROGATE_CODEPOINT_MASK;

    codepoint >>= SURROGATE_CODEPOINT_BITS;

    utf16_t high = HIGH_SURROGATE_VALUE;
    high |= codepoint & SURROGATE_CODEPOINT_MASK;

    utf16[index] = high;
    utf16[index + 1] = low;

    return 2;
}

size_t utf8_to_utf16(const unsigned char * utf8, size_t utf8_len, unsigned short* utf16, size_t utf16_len)
{
    // The next codepoint that will be written in the UTF-16 string
    // or the size of the required buffer if utf16 is NULL
    size_t utf16_index = 0;

    for (size_t utf8_index = 0; utf8_index < utf8_len; utf8_index++)
    {
        codepoint_t codepoint = decode_utf8(utf8, utf8_len, &utf8_index);

        if (utf16 == NULL)
            utf16_index += calculate_utf16_len(codepoint);
        else
            utf16_index += encode_utf16(codepoint, utf16, utf16_len, utf16_index);
    }

    return utf16_index;
}
