#ifndef _ROTATE_H
#define _ROTATE_H

/** @file
 *
 * Bit operations
 */

#include <stdint.h>

static inline __attribute__ (( always_inline )) uint8_t
rol8 ( uint8_t data, unsigned int rotation ) {
        return ( ( data << rotation ) | ( data >> ( 8 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint8_t
ror8 ( uint8_t data, unsigned int rotation ) {
        return ( ( data >> rotation ) | ( data << ( 8 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint16_t
rol16 ( uint16_t data, unsigned int rotation ) {
        return ( ( data << rotation ) | ( data >> ( 16 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint16_t
ror16 ( uint16_t data, unsigned int rotation ) {
        return ( ( data >> rotation ) | ( data << ( 16 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint32_t
rol32 ( uint32_t data, unsigned int rotation ) {
        return ( ( data << rotation ) | ( data >> ( 32 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint32_t
ror32 ( uint32_t data, unsigned int rotation ) {
        return ( ( data >> rotation ) | ( data << ( 32 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint64_t
rol64 ( uint64_t data, unsigned int rotation ) {
        return ( ( data << rotation ) | ( data >> ( 64 - rotation ) ) );
}

static inline __attribute__ (( always_inline )) uint64_t
ror64 ( uint64_t data, unsigned int rotation ) {
        return ( ( data >> rotation ) | ( data << ( 64 - rotation ) ) );
}

#endif /* _ROTATE_H */
