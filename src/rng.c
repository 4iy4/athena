#include <stdint.h>
#include <stdlib.h>

#include "bit.h"

static u64 s[4];
static u64 sm_s;

static u64 rotl(u64 x, int k)
{
	return (x << k) | (x >> (64 - k));
}

u64 rng_next(void)
{
	const u64 result = rotl(s[0] + s[3], 23) + s[0];
	const u64 t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;

	s[3] = rotl(s[3], 45);

	return result;
}

u64 rng_next_sparse(void)
{
	return rng_next() & rng_next() & rng_next();
}

static u64 sm_next(void)
{
	u64 z = sm_s;

	z += 0x9e3779b97f4a7c15;
	z  = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z  = (z ^ (z >> 27)) * 0x94d049bb133111eb;

	return z ^ (z >> 31);
}

static void sm_seed(u64 n)
{
	sm_s = n;
}

void rng_seed(u64 n)
{
	sm_seed(n);
	const size_t len = sizeof(s) / sizeof(s[0]);
	for (size_t i = 0; i < len; ++i)
		s[i] = sm_next();
}
