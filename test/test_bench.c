/*
 *  PearPC Compute Benchmark
 *
 *  Freestanding C program that exercises real computation:
 *  PRNG data generation, MD5 hashing, LZSS compression/decompression.
 *  Designed as a baseline for JIT optimization work.
 *
 *  Custom opcodes:
 *    0x00333303 - print string (r3=addr, r4=length)
 *    0x00333304 - exit (r3=exit code)
 */

#ifndef DATA_SIZE
#define DATA_SIZE (128 * 1024)
#endif

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

/* ----------------------------------------------------------------
 *  I/O primitives
 * ---------------------------------------------------------------- */

static inline void ppc_print(const char *s, unsigned len)
{
	register unsigned long r3 __asm__("r3") = (unsigned long)s;
	register unsigned long r4 __asm__("r4") = len;
	__asm__ volatile(".long 0x00333303" : : "r"(r3), "r"(r4));
}

/* ----------------------------------------------------------------
 *  String / memory helpers
 * ---------------------------------------------------------------- */

static unsigned strlen(const char *s)
{
	unsigned n = 0;
	while (s[n])
		n++;
	return n;
}

static void print_str(const char *s)
{
	ppc_print(s, strlen(s));
}

static void *memcpy(void *dst, const void *src, unsigned n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n--)
		*d++ = *s++;
	return dst;
}

static void *memset(void *dst, int c, unsigned n)
{
	unsigned char *d = (unsigned char *)dst;
	while (n--)
		*d++ = (unsigned char)c;
	return dst;
}

static int memcmp(const void *a, const void *b, unsigned n)
{
	const unsigned char *p = (const unsigned char *)a;
	const unsigned char *q = (const unsigned char *)b;
	while (n--) {
		if (*p != *q)
			return *p - *q;
		p++;
		q++;
	}
	return 0;
}

/* Print a 32-bit value as 8 hex digits */
static void print_hex32(uint32_t v)
{
	char buf[8];
	int i;
	for (i = 7; i >= 0; i--) {
		unsigned d = v & 0xf;
		buf[i] = (d < 10) ? ('0' + d) : ('a' + d - 10);
		v >>= 4;
	}
	ppc_print(buf, 8);
}

/* Print an unsigned decimal number */
static void print_dec(uint32_t v)
{
	char buf[10];
	int i = 0;
	if (v == 0) {
		ppc_print("0", 1);
		return;
	}
	while (v) {
		buf[i++] = '0' + (v % 10);
		v /= 10;
	}
	/* reverse */
	int j;
	for (j = 0; j < i / 2; j++) {
		char t = buf[j];
		buf[j] = buf[i - 1 - j];
		buf[i - 1 - j] = t;
	}
	ppc_print(buf, i);
}

/* ----------------------------------------------------------------
 *  PRNG: xorshift32
 * ---------------------------------------------------------------- */

static uint32_t prng_state = 0xDEADBEEF;

static uint32_t xorshift32(void)
{
	uint32_t x = prng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	prng_state = x;
	return x;
}

/* ----------------------------------------------------------------
 *  MD5 (RFC 1321)
 *  Big-endian safe: explicit byte-level load for LE word packing
 * ---------------------------------------------------------------- */

static uint32_t md5_leftrotate(uint32_t x, uint32_t c)
{
	return (x << c) | (x >> (32 - c));
}

/* Load a 32-bit little-endian word from a byte pointer */
static uint32_t md5_load_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Store a 32-bit value as little-endian bytes */
static void md5_store_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static const uint32_t md5_T[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf,
	0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
	0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e,
	0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6,
	0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
	0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
	0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039,
	0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
	0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d,
	0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static const uint32_t md5_s[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static void md5_hash(const uint8_t *data, uint32_t len, uint8_t digest[16])
{
	uint32_t a0 = 0x67452301;
	uint32_t b0 = 0xefcdab89;
	uint32_t c0 = 0x98badcfe;
	uint32_t d0 = 0x10325476;

	/* Compute padded length: original + 1 + padding + 8 bytes length */
	uint32_t padded_len = ((len + 8) / 64 + 1) * 64;

	/* We process block-by-block, constructing padded blocks on the fly */
	uint32_t block_idx;
	for (block_idx = 0; block_idx < padded_len; block_idx += 64) {
		uint8_t block[64];
		uint32_t i;

		/* Fill block from source data, padding, or length */
		for (i = 0; i < 64; i++) {
			uint32_t pos = block_idx + i;
			if (pos < len)
				block[i] = data[pos];
			else if (pos == len)
				block[i] = 0x80;
			else
				block[i] = 0;
		}
		/* Append bit length in last 8 bytes of final block */
		if (block_idx + 64 == padded_len) {
			uint32_t bit_len = len * 8;
			md5_store_le32(&block[56], bit_len);
			md5_store_le32(&block[60], 0); /* high 32 bits */
		}

		/* Load 16 words (little-endian) */
		uint32_t M[16];
		for (i = 0; i < 16; i++)
			M[i] = md5_load_le32(&block[i * 4]);

		uint32_t A = a0, B = b0, C = c0, D = d0;

		for (i = 0; i < 64; i++) {
			uint32_t F, g;
			if (i < 16) {
				F = (B & C) | (~B & D);
				g = i;
			} else if (i < 32) {
				F = (D & B) | (~D & C);
				g = (5 * i + 1) % 16;
			} else if (i < 48) {
				F = B ^ C ^ D;
				g = (3 * i + 5) % 16;
			} else {
				F = C ^ (B | ~D);
				g = (7 * i) % 16;
			}
			F = F + A + md5_T[i] + M[g];
			A = D;
			D = C;
			C = B;
			B = B + md5_leftrotate(F, md5_s[i]);
		}

		a0 += A;
		b0 += B;
		c0 += C;
		d0 += D;
	}

	md5_store_le32(&digest[0], a0);
	md5_store_le32(&digest[4], b0);
	md5_store_le32(&digest[8], c0);
	md5_store_le32(&digest[12], d0);
}

static void print_md5(const uint8_t digest[16])
{
	int i;
	for (i = 0; i < 16; i++) {
		char hex[2];
		hex[0] = "0123456789abcdef"[digest[i] >> 4];
		hex[1] = "0123456789abcdef"[digest[i] & 0xf];
		ppc_print(hex, 2);
	}
}

/* ----------------------------------------------------------------
 *  LZSS Compression
 *
 *  Sliding window: 4096 bytes, max match: 18, min match: 3
 *  Format: flag bytes (8 items each)
 *    flag bit 1 = literal byte
 *    flag bit 0 = 2-byte match: (offset_hi:4 | len:4), (offset_lo:8)
 * ---------------------------------------------------------------- */

#define LZSS_WIN_SIZE   4096
#define LZSS_MIN_MATCH  3
#define LZSS_MAX_MATCH  18

static uint32_t lzss_compress(const uint8_t *src, uint32_t src_len,
                              uint8_t *dst, uint32_t dst_cap)
{
	uint32_t si = 0;  /* source index */
	uint32_t di = 0;  /* dest index */

	while (si < src_len) {
		uint32_t flag_pos = di++;
		if (di > dst_cap)
			return 0;
		uint8_t flags = 0;
		int bit;

		for (bit = 0; bit < 8 && si < src_len; bit++) {
			/* Search for best match in sliding window */
			uint32_t best_len = 0;
			uint32_t best_off = 0;
			/* Max representable offset is 4095 (12 bits), not 4096 */
			uint32_t win_start = (si > LZSS_WIN_SIZE - 1) ? (si - (LZSS_WIN_SIZE - 1)) : 0;
			uint32_t j;

			for (j = win_start; j < si; j++) {
				uint32_t ml = 0;
				while (ml < LZSS_MAX_MATCH && si + ml < src_len &&
				       src[j + ml] == src[si + ml])
					ml++;
				if (ml > best_len) {
					best_len = ml;
					best_off = si - j;
				}
			}

			if (best_len >= LZSS_MIN_MATCH) {
				/* Emit match: flag bit = 0 */
				uint32_t encoded_len = best_len - LZSS_MIN_MATCH;
				if (di + 2 > dst_cap)
					return 0;
				dst[di++] = (uint8_t)(((best_off >> 8) & 0x0F) | (encoded_len << 4));
				dst[di++] = (uint8_t)(best_off & 0xFF);
				si += best_len;
			} else {
				/* Emit literal: flag bit = 1 */
				flags |= (1 << bit);
				if (di + 1 > dst_cap)
					return 0;
				dst[di++] = src[si++];
			}
		}

		dst[flag_pos] = flags;
	}

	return di;
}

static uint32_t lzss_decompress(const uint8_t *src, uint32_t src_len,
                                uint8_t *dst, uint32_t dst_cap)
{
	uint32_t si = 0;
	uint32_t di = 0;

	while (si < src_len) {
		uint8_t flags = src[si++];
		int bit;

		for (bit = 0; bit < 8; bit++) {
			if (flags & (1 << bit)) {
				/* Literal */
				if (si >= src_len)
					goto done;
				if (di >= dst_cap)
					return 0;
				dst[di++] = src[si++];
			} else {
				/* Match */
				if (si + 2 > src_len)
					goto done; /* trailing unused bits */
				uint8_t b0 = src[si++];
				uint8_t b1 = src[si++];
				uint32_t offset = ((uint32_t)(b0 & 0x0F) << 8) | b1;
				uint32_t match_len = (b0 >> 4) + LZSS_MIN_MATCH;
				if (offset == 0 || offset > di)
					return 0;
				uint32_t k;
				for (k = 0; k < match_len; k++) {
					if (di >= dst_cap)
						return 0;
					dst[di] = dst[di - offset];
					di++;
				}
			}
		}
	}
done:

	return di;
}

/* ----------------------------------------------------------------
 *  Static buffers (BSS)
 * ---------------------------------------------------------------- */

static uint8_t original_data[DATA_SIZE];
static uint8_t compressed_data[DATA_SIZE * 2];
static uint8_t decompressed_data[DATA_SIZE];

/* ----------------------------------------------------------------
 *  Main
 * ---------------------------------------------------------------- */

int main(void)
{
	uint8_t digest_orig[16];
	uint8_t digest_decomp[16];
	uint32_t i;
	int fail = 0;

	print_str("=== PearPC Compute Benchmark ===\n");
	print_str("DATA_SIZE = ");
	print_dec(DATA_SIZE);
	print_str(" bytes\n\n");

	/* Phase 1: Generate compressible data via PRNG */
	print_str("Phase 1: Generating data... ");
	i = 0;
	while (i < DATA_SIZE) {
		uint32_t r = xorshift32();
		uint8_t byte_val = (uint8_t)(r & 0x1F); /* limited alphabet */
		uint32_t run_len = (r >> 8) % 16 + 1;   /* 1-16 */
		uint32_t j;
		for (j = 0; j < run_len && i < DATA_SIZE; j++)
			original_data[i++] = byte_val;
	}
	print_str("done\n");

	/* Phase 2: MD5 of original data */
	print_str("Phase 2: MD5 of original... ");
	md5_hash(original_data, DATA_SIZE, digest_orig);
	print_md5(digest_orig);
	print_str("\n");

	/* Phase 3: LZSS compress */
	print_str("Phase 3: Compressing...     ");
	uint32_t comp_size = lzss_compress(original_data, DATA_SIZE,
	                                   compressed_data, DATA_SIZE * 2);
	if (comp_size == 0) {
		print_str("FAIL (compression returned 0)\n");
		return 1;
	}
	print_dec(comp_size);
	print_str(" bytes (");
	print_dec(comp_size * 100 / DATA_SIZE);
	print_str("%)\n");

	/* Phase 4: LZSS decompress */
	print_str("Phase 4: Decompressing...   ");
	uint32_t decomp_size = lzss_decompress(compressed_data, comp_size,
	                                       decompressed_data, DATA_SIZE);
	if (decomp_size != DATA_SIZE) {
		print_str("FAIL (size mismatch: got ");
		print_dec(decomp_size);
		print_str(")\n");
		fail = 1;
	} else {
		print_str("done\n");
	}

	/* Phase 5: MD5 of decompressed data */
	print_str("Phase 5: MD5 of decompressed... ");
	md5_hash(decompressed_data, decomp_size, digest_decomp);
	print_md5(digest_decomp);
	print_str("\n");

	/* Phase 6: Verify */
	print_str("\n");
	if (memcmp(digest_orig, digest_decomp, 16) != 0) {
		print_str("FAIL: MD5 mismatch!\n");
		fail = 1;
	}
	if (!fail && memcmp(original_data, decompressed_data, DATA_SIZE) != 0) {
		print_str("FAIL: Data mismatch!\n");
		fail = 1;
	}

	if (fail) {
		print_str("=== BENCHMARK FAILED ===\n");
		return 1;
	}

	print_str("=== BENCHMARK PASSED ===\n");
	return 0;
}
