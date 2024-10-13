#ifndef BIGINT_H
#define BIGINT_H

typedef unsigned int uint;

typedef struct bigint {
    unsigned int invalid : 1;
    unsigned int sign : 1;
    unsigned int size : 30; /* size is zero if the value is zero */
    unsigned int* digit;
} bigint_t;

extern bigint_t new_bigint(unsigned int size);
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
extern bigint_t bigint_from_tens_power(uint exp);

#endif
