#ifndef __bitops_h
#define __bitops_h

/**
 * This file defines functions to manipulate bits.
 */
#include <inttypes.h>
#include <arpa/inet.h>
#include <math.h>
#include "injector.h"

/**
 * Check is specific bit is set
 */
#define CHECK_BIT(var, pos) ((var) & ((uint64_t)1 << pos))

#define SET_BIT(var, pos) ((var) |= ((uint64_t)1 << pos))
#define UNSET_BIT(var, pos) ((var) &= ~((uint64_t)1 << pos))

/**
 * Sign extending from a constant bit-width.
 * Example: sign extend 5 bit number n to r, use:
 * int8_t r = SIGN_ENTEND(int8_t, n, 5);
 */
#define SIGN_EXTEND(T, value, width)    \
({                          \
    struct {                \
        T x : (width + 1);              \
    } s __attribute__ ((unused));       \
    s.x = value;            \
})

/**
 * Check if address is 16/32/64-bit aligned
 */
#define ALIGN_16(addr) ((addr & 0x01) == 0)
#define ALIGN_32(addr) ((addr & 0x03) == 0)
#define ALIGN_64(addr) ((addr & 0x07) == 0)

#define UNALIGNED_16(addr) ((addr & 0x01) != 0)
#define UNALIGNED_32(addr) ((addr & 0x03) != 0)
#define UNALIGNED_64(addr) ((addr & 0x07) != 0)

static uint8_t cnt_1_byte[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

/**
 * Get the position(index) of most significant bit in a 32-bit
 * integer on X86-64 architecture. Note bits are started from
 * base index 0.
 */
static inline int hibit(uint32_t n)
{
    if (n == 0) {
        return 0;
    }

    int zeros = __builtin_clz(n);
    return (32 - zeros - 1);
}


/**
 * Count bit 1 of a byte number. Used in parity check.
 *
 * @param val :number to be processed
 *
 * @return :total number of bit1 in val.
 */
static inline int cnt_bit1(uint8_t val)
{
    return cnt_1_byte[val];
}

/**
 * Count bit 1 of a 32bit number.
 *
 * @param val :number to be processed
 *
 * @return :total number of bit 1 in val.
 */
static inline int cnt_bit1_32(uint32_t val)
{
    uint8_t* p = (uint8_t*)&val;

    return cnt_1_byte[p[0]] + cnt_1_byte[p[1]]
        + cnt_1_byte[p[2]] + cnt_1_byte[p[3]];
}

/**
 * Count bit 1 of a 64bit number.
 *
 * @param val :number to be processed
 *
 * @return :total number of bit 1 in val.
 */
static inline int cnt_bit1_64(uint64_t val)
{
    uint8_t* p = (uint8_t*)&val;

    return cnt_1_byte[p[0]] + cnt_1_byte[p[1]]
        + cnt_1_byte[p[2]] + cnt_1_byte[p[3]]
        + cnt_1_byte[p[4]] + cnt_1_byte[p[5]]
        + cnt_1_byte[p[6]] + cnt_1_byte[p[7]];
}

/**
 * Below are inline functions to process endieness.
 */

static inline uint16_t byteswap16(uint16_t val)
{
#ifdef FLIP_ENDIAN
    return htobe16(val);
#else
    return val;
#endif
}

static inline uint32_t byteswap32(uint32_t val)
{

#ifdef FLIP_ENDIAN
    return htobe32(val);
#else
    return val;
#endif
}

static inline uint64_t byteswap64(uint64_t val)
{
#ifdef FLIP_ENDIAN
    return htobe64(val);
#else
    return val;
#endif
}

static inline uint64_t strswap16(char* val)
{
#ifdef FLIP_ENDIAN
    return strcat(val[1], val[0]);
#else
    return val;
#endif
}

#endif


