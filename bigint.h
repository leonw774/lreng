#include <stdlib.h>

#ifndef BIGINT_H
#define BIGINT_H

typedef unsigned long int uint32_t;
typedef unsigned long long int uint64_t;

typedef struct bigint {
    uint32_t invalid : 1;
    uint32_t sign : 1;
    uint32_t size : 30; /* size is zero if the value is zero */
    uint32_t* digit;
} bigint_t;

extern bigint_t new_bigint(uint32_t size);
extern bigint_t copy_bigint(bigint_t* x);
extern void free_bigint(bigint_t*);
extern int print_bigint(bigint_t* x);

extern bigint_t bi_add(bigint_t* a, bigint_t* b);
extern bigint_t bi_sub(bigint_t* a, bigint_t* b);
extern bigint_t bi_mul(bigint_t* a, bigint_t* b);
extern bigint_t bi_div(bigint_t* a, bigint_t* b);
extern bigint_t bi_mod(bigint_t* a, bigint_t* b);
extern bigint_t bi_eq(bigint_t* a, bigint_t* b);
extern bigint_t bi_lt(bigint_t* a, bigint_t* b);

extern bigint_t bigint_from_str(const char* str);
extern bigint_t bigint_from_tens_power(uint32_t exp);

#endif
