/* 
 * Oct 15, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Nicer crc32 functions/docs submitted by linux@horizon.com.  Thanks!
 *
 * Oct 12, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Same crc32 function was used in 5 other places in the kernel.
 * I made one version, and deleted the others.
 * There are various incantations of crc32().  Some use a seed of 0 or ~0.
 * Some xor at the end with ~0.  The generic crc32() function takes
 * seed as an argument, and doesn't xor at the end.  Then individual
 * users can do whatever they need.
 *   drivers/net/smc9194.c uses seed ~0, doesn't xor with ~0.
 *   fs/jffs2 uses seed 0, doesn't xor with ~0.
 *   fs/partitions/efi.c uses seed ~0, xor's with ~0.
 * 
 */

#include "system/types.h"
typedef uint32 u32;
typedef uint8 u8;

#include "crc32.h"

// FIXME: HACKED / host endianess
//#define __LITTLE_ENDIAN

#include "crc32defs.h"
#if CRC_LE_BITS == 8

// FIXME: host endianess
#define tole(x) x
//__constant_cpu_to_le32(x)

// FIXME: host endianess
#define tobe(x) 0
//__constant_cpu_to_be32(x)
#else
#define tole(x) (x)
#define tobe(x) (x)
#endif

// FIXME: host endianess
#define __cpu_to_le32(x) (x)
#define __le32_to_cpu(x) (x)
#define __cpu_to_be32(x) (x)
#define __be32_to_cpu(x) (x)

#define likely(x) (x)
#define unlikely(x) (x)

#include "crc32table.h"

#if __GNUC__ >= 3	/* 2.x has "attribute", but only 3.0 has "pure */
#define attribute(x) __attribute__(x)
#else
#define attribute(x)
#endif

/*
 * This code is in the public domain; copyright abandoned.
 * Liability for non-performance of this code is limited to the amount
 * you paid for it.  Since it is distributed for free, your refund will
 * be very very small.  If it breaks, you get to keep both pieces.
 */

#if CRC_LE_BITS == 1
/*
 * In fact, the table-based code will work in this case, but it can be
 * simplified by inlining the table in ?: form.
 */

/**
 * crc32_le() - Calculate bitwise little-endian Ethernet AUTODIN II CRC32
 * @crc - seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *        other uses, or the previous crc32 value if computing incrementally.
 * @p   - pointer to buffer over which CRC is run
 * @len - length of buffer @p
 * 
 */
u32 attribute((pure)) crc32_le(u32 crc, unsigned char const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
	}
	return crc;
}
#else				/* Table-based approach */

/**
 * crc32_le() - Calculate bitwise little-endian Ethernet AUTODIN II CRC32
 * @crc - seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *        other uses, or the previous crc32 value if computing incrementally.
 * @p   - pointer to buffer over which CRC is run
 * @len - length of buffer @p
 * 
 */
u32 attribute((pure)) crc32_le(u32 crc, unsigned char const *p, size_t len)
{
# if CRC_LE_BITS == 8
	const u32      *b =(u32 *)p;
	const u32      *tab = crc32table_le;

# ifdef __LITTLE_ENDIAN
#  define DO_CRC(x) crc = tab[ (crc ^ (x)) & 255 ] ^ (crc>>8)
# else
#  define DO_CRC(x) crc = tab[ ((crc >> 24) ^ (x)) & 255] ^ (crc<<8)
# endif

	crc = __cpu_to_le32(crc);
	/* Align it */
	if(unlikely(((long)b)&3 && len)){
		do {
			u8 *p = (u8 *)b;
			DO_CRC(*p++);
			b = (const u32 *)p;
		} while ((--len) && ((long)b)&3 );
	}
	if(likely(len >= 4)){
		/* load data 32 bits wide, xor data 32 bits wide. */
		size_t save_len = len & 3;
	        len = len >> 2;
		--b; /* use pre increment below(*++b) for speed */
		do {
			crc ^= *++b;
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
		} while (--len);
		b++; /* point to next byte(s) */
		len = save_len;
	}
	/* And the last few bytes */
	if(len){
		do {
			u8 *p = (u8 *)b;
			DO_CRC(*p++);
			b = (const u32 *)p;
		} while (--len);
	}

	return __le32_to_cpu(crc);
#undef ENDIAN_SHIFT
#undef DO_CRC

# elif CRC_LE_BITS == 4
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 4) ^ crc32table_le[crc & 15];
		crc = (crc >> 4) ^ crc32table_le[crc & 15];
	}
	return crc;
# elif CRC_LE_BITS == 2
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
	}
	return crc;
# endif
}
#endif

#if CRC_BE_BITS == 1
/*
 * In fact, the table-based code will work in this case, but it can be
 * simplified by inlining the table in ?: form.
 */

/**
 * crc32_be() - Calculate bitwise big-endian Ethernet AUTODIN II CRC32
 * @crc - seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *        other uses, or the previous crc32 value if computing incrementally.
 * @p   - pointer to buffer over which CRC is run
 * @len - length of buffer @p
 * 
 */
u32 attribute((pure)) crc32_be(u32 crc, unsigned char const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++ << 24;
		for (i = 0; i < 8; i++)
			crc =
			    (crc << 1) ^ ((crc & 0x80000000) ? CRCPOLY_BE :
					  0);
	}
	return crc;
}

#else				/* Table-based approach */
/**
 * crc32_be() - Calculate bitwise big-endian Ethernet AUTODIN II CRC32
 * @crc - seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *        other uses, or the previous crc32 value if computing incrementally.
 * @p   - pointer to buffer over which CRC is run
 * @len - length of buffer @p
 * 
 */
u32 attribute((pure)) crc32_be(u32 crc, unsigned char const *p, size_t len)
{
# if CRC_BE_BITS == 8
	const u32      *b =(u32 *)p;
	const u32      *tab = crc32table_be;

# ifdef __LITTLE_ENDIAN
#  define DO_CRC(x) crc = tab[ (crc ^ (x)) & 255 ] ^ (crc>>8)
# else
#  define DO_CRC(x) crc = tab[ ((crc >> 24) ^ (x)) & 255] ^ (crc<<8)
# endif

	crc = __cpu_to_be32(crc);
	/* Align it */
	if(unlikely(((long)b)&3 && len)){
		do {
			u8 *p = (u8 *)b;
			DO_CRC(*p++);
			b = (u32 *)p;
		} while ((--len) && ((long)b)&3 );
	}
	if(likely(len >= 4)){
		/* load data 32 bits wide, xor data 32 bits wide. */
		size_t save_len = len & 3;
	        len = len >> 2;
		--b; /* use pre increment below(*++b) for speed */
		do {
			crc ^= *++b;
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
		} while (--len);
		b++; /* point to next byte(s) */
		len = save_len;
	}
	/* And the last few bytes */
	if(len){
		do {
			u8 *p = (u8 *)b;
			DO_CRC(*p++);
			b = (const u32 *)p;
		} while (--len);
	}
	return __be32_to_cpu(crc);
#undef ENDIAN_SHIFT
#undef DO_CRC

# elif CRC_BE_BITS == 4
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 4) ^ crc32table_be[crc >> 28];
		crc = (crc << 4) ^ crc32table_be[crc >> 28];
	}
	return crc;
# elif CRC_BE_BITS == 2
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
	}
	return crc;
# endif
}
#endif

u32 bitreverse(u32 x)
{
	x = (x >> 16) | (x << 16);
	x = (x >> 8 & 0x00ff00ff) | (x << 8 & 0xff00ff00);
	x = (x >> 4 & 0x0f0f0f0f) | (x << 4 & 0xf0f0f0f0);
	x = (x >> 2 & 0x33333333) | (x << 2 & 0xcccccccc);
	x = (x >> 1 & 0x55555555) | (x << 1 & 0xaaaaaaaa);
	return x;
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
 * 	multiple = remainder & 0x80000000 ? CRCPOLY : 0;
 * 	remainder = (remainder << 1 | next_input_bit()) ^ multiple;
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
 * 	multiple = (remainder & 0x80000000) ? CRCPOLY : 0;
 * 	remainder = (remainder << 1) ^ multiple;
 * }
 * With this optimization, the little-endian code is simpler:
 * for (i = 0; i < input_bits; i++) {
 *      remainder ^= next_input_bit();
 * 	multiple = (remainder & 1) ? CRCPOLY : 0;
 * 	remainder = (remainder >> 1) ^ multiple;
 * }
 *
 * Note that the other details of endianness have been hidden in CRCPOLY
 * (which must be bit-reversed) and next_input_bit().
 *
 * However, as long as next_input_bit is returning the bits in a sensible
 * order, we can actually do the merging 8 or more bits at a time rather
 * than one bit at a time:
 * for (i = 0; i < input_bytes; i++) {
 * 	remainder ^= next_input_byte() << 24;
 * 	for (j = 0; j < 8; j++) {
 * 		multiple = (remainder & 0x80000000) ? CRCPOLY : 0;
 * 		remainder = (remainder << 1) ^ multiple;
 * 	}
 * }
 * Or in little-endian:
 * for (i = 0; i < input_bytes; i++) {
 * 	remainder ^= next_input_byte();
 * 	for (j = 0; j < 8; j++) {
 * 		multiple = (remainder & 1) ? CRCPOLY : 0;
 * 		remainder = (remainder << 1) ^ multiple;
 * 	}
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
