/*
 * Function for computing CRC32 for the purpose of adding to Ethernet packets.
 *
 */

#include "crc32.h"

static const uint32 crc32table[0x100] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
    0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
    0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
    0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
    0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
    0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
    0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
    0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
    0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
    0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
    0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
    0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
    0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
    0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
    0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
    0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
    0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
    0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
    0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
    0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
    0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
    0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
    0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
    0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
    0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
    0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
    0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
    0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
    0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
    0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
    0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
    0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
    0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
    0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
    0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
    0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
    0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
    0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
    0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
    0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
    0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
    0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
    0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
    0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
    0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
    0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
    0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
    0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
    0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
    0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
    0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
    0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};


// The previous table could have been built using the following function :

/*

#define CRC32_POLY 0xedb88320;          // this is a 0x04c11db7 reflection

void init_crc32()
{
    int i, j, b;
    uint32 c;

    for (i = 0; i < 0x100; i++) {
        for (c = i, j = 0; j < 8; j++) {
            b = c & 1;
            c >>= 1;
            if (b)
                c ^= CRC32_POLY;
        }
        crc32table[i] = c;
    }
}
*/

// With this macro defined, the function runs about 35% faster, but the code is about 3 times bigger :
#define RUN_FASTER

#define DO_CRC(b) crc = (crc >> 8) ^ crc32table[(crc & 0xff) ^ (b)]

uint32 ether_crc(size_t len, const byte *p)
{
    uint32  crc = 0xffffffff;   // preload shift register, per CRC-32 spec

    for (; len>0; len--) {
        DO_CRC(*p++);
    }
    return ~crc;                // transmit complement, per CRC-32 spec
}

/*
 * A brief CRC tutorial.
 *
 * A CRC is a long-division remainder.  You add the CRC to the message,
 * and the whole thing (message+CRC) is a multiple of the given
 * CRC polynomial.  To check the CRC, you can either check that the
 * CRC matches the recomputed value, *or* you can check that the
 * remainder computed on the message+CRC is 0.  This latter approach
 * is used by a lot of hardware implementations, and is why so many
 * protocols put the end-of-frame flag after the CRC.
 *
 * It's actually the same long division you learned in school, except that
 * - We're working in binary, so the digits are only 0 and 1, and
 * - When dividing polynomials, there are no carries.  Rather than add and
 *   subtract, we just xor.  Thus, we tend to get a bit sloppy about
 *   the difference between adding and subtracting.
 *
 * A 32-bit CRC polynomial is actually 33 bits long.  But since it's
 * 33 bits long, bit 32 is always going to be set, so usually the CRC
 * is written in hex with the most significant bit omitted.  (If you're
 * familiar with the IEEE 754 floating-point format, it's the same idea.)
 *
 * Note that a CRC is computed over a string of *bits*, so you have
 * to decide on the endianness of the bits within each byte.  To get
 * the best error-detecting properties, this should correspond to the
 * order they're actually sent.  For example, standard RS-232 serial is
 * little-endian; the most significant bit (sometimes used for parity)
 * is sent last.  And when appending a CRC word to a message, you should
 * do it in the right order, matching the endianness.
 *
 * Just like with ordinary division, the remainder is always smaller than
 * the divisor (the CRC polynomial) you're dividing by.  Each step of the
 * division, you take one more digit (bit) of the dividend and append it
 * to the current remainder.  Then you figure out the appropriate multiple
 * of the divisor to subtract to being the remainder back into range.
 * In binary, it's easy - it has to be either 0 or 1, and to make the
 * XOR cancel, it's just a copy of bit 32 of the remainder.
 *
 * When computing a CRC, we don't care about the quotient, so we can
 * throw the quotient bit away, but subtract the appropriate multiple of
 * the polynomial from the remainder and we're back to where we started,
 * ready to process the next bit.
 *
 * A big-endian CRC written this way would be coded like:
 * for (i = 0; i < input_bits; i++) {
 *  multiple = remainder & 0x80000000 ? CRCPOLY : 0;
 *  remainder = (remainder << 1 | next_input_bit()) ^ multiple;
 * }
 * Notice how, to get at bit 32 of the shifted remainder, we look
 * at bit 31 of the remainder *before* shifting it.
 *
 * But also notice how the next_input_bit() bits we're shifting into
 * the remainder don't actually affect any decision-making until
 * 32 bits later.  Thus, the first 32 cycles of this are pretty boring.
 * Also, to add the CRC to a message, we need a 32-bit-long hole for it at
 * the end, so we have to add 32 extra cycles shifting in zeros at the
 * end of every message,
 *
 * So the standard trick is to rearrage merging in the next_input_bit()
 * until the moment it's needed.  Then the first 32 cycles can be precomputed,
 * and merging in the final 32 zero bits to make room for the CRC can be
 * skipped entirely.
 * This changes the code to:
 * for (i = 0; i < input_bits; i++) {
 *      remainder ^= next_input_bit() << 31;
 *  multiple = (remainder & 0x80000000) ? CRCPOLY : 0;
 *  remainder = (remainder << 1) ^ multiple;
 * }
 * With this optimization, the little-endian code is simpler:
 * for (i = 0; i < input_bits; i++) {
 *      remainder ^= next_input_bit();
 *  multiple = (remainder & 1) ? CRCPOLY : 0;
 *  remainder = (remainder >> 1) ^ multiple;
 * }
 *
 * Note that the other details of endianness have been hidden in CRCPOLY
 * (which must be bit-reversed) and next_input_bit().
 *
 * However, as long as next_input_bit is returning the bits in a sensible
 * order, we can actually do the merging 8 or more bits at a time rather
 * than one bit at a time:
 * for (i = 0; i < input_bytes; i++) {
 *  remainder ^= next_input_byte() << 24;
 *  for (j = 0; j < 8; j++) {
 *      multiple = (remainder & 0x80000000) ? CRCPOLY : 0;
 *      remainder = (remainder << 1) ^ multiple;
 *  }
 * }
 * Or in little-endian:
 * for (i = 0; i < input_bytes; i++) {
 *  remainder ^= next_input_byte();
 *  for (j = 0; j < 8; j++) {
 *      multiple = (remainder & 1) ? CRCPOLY : 0;
 *      remainder = (remainder >> 1) ^ multiple;
 *  }
 * }
 * If the input is a multiple of 32 bits, you can even XOR in a 32-bit
 * word at a time and increase the inner loop count to 32.
 *
 * You can also mix and match the two loop styles, for example doing the
 * bulk of a message byte-at-a-time and adding bit-at-a-time processing
 * for any fractional bytes at the end.
 *
 * The only remaining optimization is to the byte-at-a-time table method.
 * Here, rather than just shifting one bit of the remainder to decide
 * in the correct multiple to subtract, we can shift a byte at a time.
 * This produces a 40-bit (rather than a 33-bit) intermediate remainder,
 * but again the multiple of the polynomial to subtract depends only on
 * the high bits, the high 8 bits in this case.
 *
 * The multile we need in that case is the low 32 bits of a 40-bit
 * value whose high 8 bits are given, and which is a multiple of the
 * generator polynomial.  This is simply the CRC-32 of the given
 * one-byte message.
 *
 * Two more details: normally, appending zero bits to a message which
 * is already a multiple of a polynomial produces a larger multiple of that
 * polynomial.  To enable a CRC to detect this condition, it's common to
 * invert the CRC before appending it.  This makes the remainder of the
 * message+crc come out not as zero, but some fixed non-zero value.
 *
 * The same problem applies to zero bits prepended to the message, and
 * a similar solution is used.  Instead of starting with a remainder of
 * 0, an initial remainder of all ones is used.  As long as you start
 * the same way on decoding, it doesn't make a difference.
 */
