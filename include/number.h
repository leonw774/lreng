#include "bigint.h"

#ifndef NUMBER_H
#define NUMBER_H

/* number are normalized as such:
   no flag is set
    - sign: 0 or 1
    - zero: 0
    - nan: 0
    - numer: non-negative
    - denom: positive

   is zero:
    - sign: 0
    - zero: 1
    - nan: 0
    - numer, denom: 0

   is nan:
    - sign: 0
    - zero: 0
    - nan: 1
    - numer, demon: nan
*/
typedef struct number {
    bigint_t numer;
    bigint_t denom;
} number_t;

#define number_struct_size sizeof(number_t)

#define EMPTY_NUMBER() ((number_t) { \
    .numer = ZERO_BIGINT(), .denom = ZERO_BIGINT()})
#define ZERO_NUMBER() ((number_t) { \
    .numer = ZERO_BIGINT(), .denom = ZERO_BIGINT()})
#define ONE_NUMBER() ((number_t) { \
    .numer = ONE_BIGINT(), .denom = ONE_BIGINT()})
#define NAN_NUMBER() ((number_t) { \
    .numer = NAN_BIGINT(), .denom = NAN_BIGINT()})

extern void copy_number(number_t* dst, const number_t* src);
extern void free_number(number_t* x);

extern int number_eq(number_t* a, number_t* b);
extern int number_lt(number_t* a, number_t* b);

extern number_t number_add(number_t* a, number_t* b);
extern number_t number_sub(number_t* a, number_t* b);
extern number_t number_mul(number_t* a, number_t* b);
extern number_t number_div(number_t* a, number_t* b);
extern number_t number_mod(number_t* a, number_t* b);
extern number_t number_exp(number_t* a, number_t* b);

extern int print_number_frac(number_t* x);
extern int print_number_dec(number_t* x, int precision);
extern number_t number_from_str(const char* str);
extern number_t number_from_i32(i32 n);

#endif
