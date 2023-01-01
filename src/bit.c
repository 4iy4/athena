#include <stdint.h>

#include "bit.h"

int count_bits(u64 n)
{
#if defined(__clang__) || defined(__GNUC__)
	return __builtin_popcountll(n);
#else
	const u64 m1  = 0x5555555555555555;
	const u64 m2  = 0x3333333333333333;
	const u64 m4  = 0x0f0f0f0f0f0f0f0f;
	const u64 m8  = 0x00ff00ff00ff00ff;
	const u64 m16 = 0x0000ffff0000ffff;
	const u64 m32 = 0x00000000ffffffff;
	const u64 h01 = 0x0101010101010101;

	x -= (x >> 1) & m1;
	x = (x & m2) + ((x >> 2) & m2);
	x = (x + (x >> 4)) & m4;
	return (x * h01) >> 56;
#endif
}

/*
 * Returns the index of the first bit set to 1
 * and then set it to 0.
 */
int get_index_of_first_bit_and_unset(u64 *n)
{
	const int i = get_index_of_first_bit(*n);
	*n &= *n - 1;
	return i;
}

int get_index_of_first_bit(u64 n)
{
#if defined(__clang__) || defined(__GNUC__)
	return __builtin_ffsll(n) - 1;
#else
	const int index[64] = {
		0,  47,  1, 56, 48, 27,  2, 60,
		57, 49, 41, 37, 28, 16,  3, 61,
		54, 58, 35, 52, 50, 42, 21, 44,
		38, 32, 29, 23, 17, 11,  4, 62,
		46, 55, 26, 59, 40, 36, 15, 53,
		34, 51, 20, 43, 31, 22, 10, 45,
		25, 39, 14, 33, 19, 30,  9, 24,
		13, 18,  8, 12,  7,  6,  5, 63,
	};
	const u64 debruijn = U64(0x03f79d71b4cb0a89);

	return index[((n ^ (n - 1)) * debruijn) >> 58];
#endif
}

int get_index_of_last_bit(u64 n) {
#if defined(__clang__) || defined(__GNUC__)
	return 63 - __builtin_clzll(n);
#else
	/* This assumes little endian. */
	union {
		double d;
		struct {
			unsigned int mantissal: 32;
			unsigned int mantissah: 20;
			unsigned int exponent: 11;
			unsigned int sign: 1;
		};
	} ud;
	ud.d = (double)(bb & ~(bb >> 32));
	return ud.exponent - 1023;
#endif
}
