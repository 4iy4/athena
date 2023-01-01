#ifndef RNG_H
#define RNG_H

void rng_seed(u64 n);
u64 rng_next(void);
u64 rng_next_sparse(void);

#endif
