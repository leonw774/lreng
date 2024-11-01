#include <stdint.h>
#include "dynarr.h"

#ifndef BIGINT_H
#define BIGINT_H

typedef int32_t i32;
typedef int64_t i64;
typedef uint32_t u32;
typedef uint64_t u64;


typedef struct bigint {
    u32 size : 30; /* size is zero if the value is zero */
    u32 nan : 1;
    u32 sign : 1;
    u32* digit;
} bigint_t;

#define ZERO_BIGINT() ((bigint_t) {.nan = 0, .sign = 0, .size = 0, .digit = 0})
#define NAN_BIGINT() ((bigint_t) {.nan = 1, .sign = 0, .size = 0, .digit = 0})
extern bigint_t ONE_BIGINT();

extern void bi_new(bigint_t* x, u32 size);
extern void bi_copy(bigint_t* dst, bigint_t* src);
extern void bi_free(bigint_t* x);

extern int bi_eq(bigint_t* a, bigint_t* b);
extern int bi_lt(bigint_t* a, bigint_t* b);

extern bigint_t bi_add(bigint_t* a, bigint_t* b);
extern bigint_t bi_sub(bigint_t* a, bigint_t* b);
extern bigint_t bi_mul(bigint_t* a, bigint_t* b);
extern bigint_t bi_div(bigint_t* a, bigint_t* b);
extern bigint_t bi_mod(bigint_t* a, bigint_t* b);

extern int print_bigint(bigint_t* x);
dynarr_t bigint_to_dec_string(bigint_t* x);
extern int print_bigint_dec(bigint_t* x);
extern bigint_t bigint_from_str(const char* str);
extern bigint_t bigint_from_tens_power(u32 exp);

#endif
