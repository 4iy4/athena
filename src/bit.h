#ifndef BIT_H
#define BIT_H

#define U64(n) UINT64_C(n)

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

int count_bits(u64 n);
int get_index_of_first_bit(u64 n);
int get_index_of_first_bit_and_unset(u64 *n);
int get_index_of_last_bit(u64 n);

#endif
