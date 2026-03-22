/*
 * Native LZSS test - extracted from test_bench.c
 * Verifies compress/decompress round-trip on the host.
 * If this passes natively but fails on PearPC, the bug is endian-related.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DATA_SIZE (128 * 1024)

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
 *  LZSS Compression (verbatim from test_bench.c)
 * ---------------------------------------------------------------- */

#define LZSS_WIN_SIZE   4096
#define LZSS_MIN_MATCH  3
#define LZSS_MAX_MATCH  18

static uint32_t lzss_compress(const uint8_t *src, uint32_t src_len,
                              uint8_t *dst, uint32_t dst_cap)
{
	uint32_t si = 0;
	uint32_t di = 0;

	while (si < src_len) {
		uint32_t flag_pos = di++;
		if (di > dst_cap)
			return 0;
		uint8_t flags = 0;
		int bit;

		for (bit = 0; bit < 8 && si < src_len; bit++) {
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
				uint32_t encoded_len = best_len - LZSS_MIN_MATCH;
				if (di + 2 > dst_cap)
					return 0;
				dst[di++] = (uint8_t)(((best_off >> 8) & 0x0F) | (encoded_len << 4));
				dst[di++] = (uint8_t)(best_off & 0xFF);
				si += best_len;
			} else {
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
					goto done;
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
 *  Static buffers
 * ---------------------------------------------------------------- */

static uint8_t original_data[DATA_SIZE];
static uint8_t compressed_data[DATA_SIZE * 2];
static uint8_t decompressed_data[DATA_SIZE];

int main(void)
{
	uint32_t i;

	printf("=== Native LZSS Round-Trip Test ===\n");
	printf("DATA_SIZE = %u bytes\n\n", DATA_SIZE);

	/* Phase 1: Generate compressible data via PRNG (same as test_bench.c) */
	printf("Phase 1: Generating data... ");
	i = 0;
	while (i < DATA_SIZE) {
		uint32_t r = xorshift32();
		uint8_t byte_val = (uint8_t)(r & 0x1F);
		uint32_t run_len = (r >> 8) % 16 + 1;
		uint32_t j;
		for (j = 0; j < run_len && i < DATA_SIZE; j++)
			original_data[i++] = byte_val;
	}
	printf("done\n");

	/* Phase 2: Compress */
	printf("Phase 2: Compressing...     ");
	uint32_t comp_size = lzss_compress(original_data, DATA_SIZE,
	                                   compressed_data, DATA_SIZE * 2);
	if (comp_size == 0) {
		printf("FAIL (compression returned 0)\n");
		return 1;
	}
	printf("%u bytes (%u%%)\n", comp_size, comp_size * 100 / DATA_SIZE);

	/* Phase 3: Decompress */
	printf("Phase 3: Decompressing...   ");
	uint32_t decomp_size = lzss_decompress(compressed_data, comp_size,
	                                       decompressed_data, DATA_SIZE);
	if (decomp_size == 0) {
		printf("FAIL (decompress returned 0)\n");

		/* Detailed diagnosis: re-run decompress step by step */
		printf("\n--- Diagnosis ---\n");
		printf("Compressed stream starts: ");
		for (i = 0; i < 32 && i < comp_size; i++)
			printf("%02x ", compressed_data[i]);
		printf("\n");

		/* Find where it fails by decompressing manually */
		uint32_t si = 0, di = 0;
		uint32_t step = 0;
		while (si < comp_size && step < 2000) {
			uint32_t flag_si = si;
			uint8_t flags = compressed_data[si++];
			int bit;
			for (bit = 0; bit < 8; bit++) {
				if (flags & (1 << bit)) {
					if (si >= comp_size) { printf("  step %u: literal hit end of stream si=%u\n", step, si); goto diag_done; }
					if (di >= DATA_SIZE) {
						printf("  step %u: literal overflow at di=%u\n", step, di);
						goto diag_done;
					}
					if (step < 20)
						printf("  step %u: literal 0x%02x (si=%u di=%u)\n", step, compressed_data[si], si, di);
					si++; di++;
				} else {
					if (si + 2 > comp_size) { printf("  step %u: match hit end of stream si=%u\n", step, si); goto diag_done; }
					uint8_t b0 = compressed_data[si++];
					uint8_t b1 = compressed_data[si++];
					uint32_t offset = ((uint32_t)(b0 & 0x0F) << 8) | b1;
					uint32_t match_len = (b0 >> 4) + LZSS_MIN_MATCH;
					if (offset == 0 || offset > di) {
						printf("  step %u: BAD match at si=%u di=%u: flags=0x%02x(bit%d) b0=0x%02x b1=0x%02x offset=%u (>di=%u) match_len=%u\n",
						       step, si - 2, di, flags, bit, b0, b1, offset, di, match_len);
						/* Show surrounding compressed bytes */
						printf("  compressed[%u..%u]: ", si > 10 ? si - 10 : 0, si + 5 < comp_size ? si + 5 : comp_size - 1);
						uint32_t k;
						for (k = (si > 10 ? si - 10 : 0); k <= (si + 5 < comp_size ? si + 5 : comp_size - 1); k++)
							printf("%02x ", compressed_data[k]);
						printf("\n");
						goto diag_done;
					}
					if (step < 20)
						printf("  step %u: match offset=%u len=%u (si=%u di=%u)\n", step, offset, match_len, si - 2, di);
					di += match_len;
				}
				step++;
			}
		}
		printf("  (traced %u steps without error, si=%u di=%u)\n", step, si, di);
		diag_done:
		return 1;
	}

	if (decomp_size != DATA_SIZE) {
		printf("FAIL (size mismatch: got %u, expected %u)\n", decomp_size, DATA_SIZE);
	} else {
		printf("done (%u bytes)\n", decomp_size);
	}

	/* Phase 4: Verify */
	if (decomp_size == DATA_SIZE && memcmp(original_data, decompressed_data, DATA_SIZE) == 0) {
		printf("\n=== PASS: round-trip matches ===\n");
		return 0;
	}

	/* Find first divergence */
	printf("\n--- Data mismatch ---\n");
	uint32_t limit = decomp_size < DATA_SIZE ? decomp_size : DATA_SIZE;
	for (i = 0; i < limit; i++) {
		if (original_data[i] != decompressed_data[i]) {
			printf("First divergence at offset %u: original=0x%02x decompressed=0x%02x\n",
			       i, original_data[i], decompressed_data[i]);
			/* Print context around the divergence */
			uint32_t start = (i > 8) ? i - 8 : 0;
			uint32_t end = (i + 8 < limit) ? i + 8 : limit;
			printf("  original[%u..%u]:     ", start, end - 1);
			uint32_t k;
			for (k = start; k < end; k++)
				printf("%02x ", original_data[k]);
			printf("\n  decompressed[%u..%u]: ", start, end - 1);
			for (k = start; k < end; k++)
				printf("%02x ", decompressed_data[k]);
			printf("\n");
			break;
		}
	}
	if (decomp_size != DATA_SIZE)
		printf("Size mismatch: got %u, expected %u\n", decomp_size, DATA_SIZE);

	printf("\n=== FAIL ===\n");
	return 1;
}
