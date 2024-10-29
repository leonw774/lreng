#include "../utils/bigint.h"

#ifndef NUMBER_H
#define NUMBER_H

#define NUMBER_FLAG_NAN  0x1
#define NUMBER_FLAG_INF  0x2
#define NUMBER_FLAG_NINF 0x4

/* number are normalized as such:
   no flag is set
    - sign: 0 or 1
    - zero: 0
    - flag: 0
    - numer: non-negative
    - denom: positive

   is zero:
    - sign: 0
    - zero: 1
    - flag: 0
    - numer, denom: 0

   flag is set:
    - sign: 0
    - zero: 0
    - flag: only one is set
    - numer, demon: nan
*/
typedef struct number {
    unsigned char sign : 1;
    unsigned char zero : 1;
    unsigned char flag : 6;
    bigint_t numer;
    bigint_t denom;
} number_t;

#define ZERO_NUMBER() ((number_t) { \
    .sign = 0, .zero = 1, .flag = 0, \
    .numer = ZERO_BIGINT(), .denom = ZERO_BIGINT()})
#define NAN_NUMBER() ((number_t) { \
    .sign = 0, .zero = 0, .flag = NUMBER_FLAG_NAN, \
    .numer = NAN_BIGINT(), .denom = NAN_BIGINT()})

#define NUMPTR_IS_NAN(x) (x->flag | NUMBER_FLAG_NAN) 
#define NUMPTR_IS_INF(x) (x->flag | NUMBER_FLAG_INF) 
#define NUMPTR_IS_NINF(x) (x->flag | NUMBER_FLAG_NINF)

extern void number_copy(number_t* dst, number_t* src);
extern void number_free(number_t* x);

extern int number_eq(number_t* a, number_t* b);
extern int number_lt(number_t* a, number_t* b);

extern number_t number_add(number_t* a, number_t* b);
extern number_t number_sub(number_t* a, number_t* b);
extern number_t number_mul(number_t* a, number_t* b);
extern number_t number_div(number_t* a, number_t* b);
extern number_t number_mod(number_t* a, number_t* b);

extern int print_number_frac(number_t* x);
extern int print_number_dec(number_t* x, int digit_num);
extern number_t number_from_str(const char* str);
extern number_t number_from_bigint(bigint_t* str);

#endif
