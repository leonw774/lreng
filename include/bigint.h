#include "dynarr.h"
#include <stdint.h>

#ifndef BIGINT_H
#define BIGINT_H

#define BASE_SHIFT 31
#define DIGIT_BASE ((u32)1 << BASE_SHIFT)
#define CARRY_MASK ((u32)1 << BASE_SHIFT)
#define DIGIT_MASK (((u32)1 << BASE_SHIFT) - 1)

typedef uint8_t u8;
typedef int32_t i32;
typedef int64_t i64;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct bigint {
    u8 sign;
    u8 nan;
    u8 size; /* size is zero if the value is zero */
    u8 shared;
    u32* digit;
} bigint_t;

#define bigint_struct_size sizeof(bigint_t)

#define ZERO_BIGINT                                                            \
    ((bigint_t) { .sign = 0, .nan = 0, .size = 0, .shared = 0, .digit = 0 })
#define NAN_BIGINT()                                                           \
    ((bigint_t) { .sign = 0, .nan = 1, .size = 0, .shared = 0, .digit = 0 })
extern bigint_t BYTE_BIGINT(unsigned int b);

extern void bi_new(bigint_t* x, u32 size);
extern void bi_copy(bigint_t* dst, const bigint_t* src);
extern void bi_free(bigint_t* x);

extern int bi_eq(bigint_t* a, bigint_t* b);
extern int bi_lt(bigint_t* a, bigint_t* b);

extern bigint_t bi_add(bigint_t* a, bigint_t* b);
extern bigint_t bi_sub(bigint_t* a, bigint_t* b);
extern bigint_t bi_mul(bigint_t* a, bigint_t* b);
extern bigint_t bi_div(bigint_t* a, bigint_t* b);
extern bigint_t bi_mod(bigint_t* a, bigint_t* b);

extern int print_bi(bigint_t* x, char end);
extern dynarr_t bi_to_dec_str(const bigint_t* x);
extern int print_bi_dec(const bigint_t* x, char end);
extern bigint_t bi_from_str(const char* str);
extern bigint_t bi_from_tens_power(i32 exp);

#endif
