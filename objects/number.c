#include <stdio.h>
#include <string.h>
#include "number.h"

void
number_copy(number_t* dst, number_t* src) {
    dst->sign = src->sign;
    dst->zero = src->zero;
    dst->flag = src->flag;
    if (src->flag) {
        dst->numer = NAN_BIGINT();
        dst->denom = NAN_BIGINT();
    }
    else if (src->zero) {
        dst->numer = ZERO_BIGINT();
        dst->denom = ZERO_BIGINT();
    }
    else {
        bi_copy(&dst->numer, &src->numer);
        bi_copy(&dst->denom, &src->denom);
    }
}

void
number_free(number_t* x) {
    x->sign = x->zero = x->flag = 0;
    bi_free(&x->numer);
    bi_free(&x->denom);
}

void
number_normalize(number_t* x) {
    bigint_t a, b, t1, t2, one = ONE_BIGINT();
    /* flags */
    if (x->flag) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        x->numer = NAN_BIGINT();
        x->denom = NAN_BIGINT();
        x->sign = x->zero = 0;
    }

    /* normalize the sign */
    if (x->numer.sign != x->denom.sign) {
        x->sign = !x->sign;
    }
    x->numer.sign = x->denom.sign = 0;

    /* special cases: n == 0, n == 1, d == 0, d == 1, n == d */
    if (x->numer.size == 0) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        *x = ZERO_NUMBER();
        return;
    }
    if (bi_eq(&x->numer, &one) || bi_eq(&x->denom, &one)) {
        return;
    }
    if (x->denom.size == 0) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        *x = NAN_NUMBER();
        return;
    }
    if (bi_eq(&x->numer, &x->denom)) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        x->numer = ONE_BIGINT();
        x->denom = ONE_BIGINT();
        return;
    }
    

    /* euclidian algorithm */
    bi_copy(&a, &x->numer);
    bi_copy(&b, &x->denom);
    while (b.size != 0) {
        bi_free(&t1);
        bi_copy(&t1, &b); /* t1 = b */
        t2 = bi_mod(&a, &b); /* t2 = a mod b */
        bi_free(&b);
        bi_copy(&b, &t2);  /* b = t2 */
        bi_free(&a);
        bi_copy(&a, &t1); /* a = t1 */
    }

    /* a is gcd of numer & denom */
    if (!bi_eq(&a, &one)) {
        bi_free(&t1);
        t1 = bi_div(&x->numer, &a);
        bi_free(&x->numer);
        x->numer = t1;
        bi_free(&t2);
        t2 = bi_div(&x->numer, &a);
        bi_free(&x->denom);
        x->denom = t2;
        /* dont need to free t1 and ts because they are own by x now */
    }
    else {
        bi_free(&t1);
        bi_free(&t2);
    }
}

int
number_eq(number_t* a, number_t* b) {
    if (a->zero == b->zero) {
        return 1;
    }
    if (a->flag || b->flag) {
        return 0;
    }
    return (a->sign == b->sign
        && bi_eq(&a->numer, &b->numer) && bi_eq(&a->denom, &b->denom));
}

int
number_lt(number_t* a, number_t* b) {
    /* obvious cases */
    if ((a->sign != b->sign)) {
        return a->sign;
    }
    if (bi_eq(&a->denom, &b->denom)) {
        if (a->sign) {
            return bi_lt(&b->numer, &a->numer);
        }
        else {
            return bi_lt(&a->numer, &b->numer);
        }
    }
    if (bi_eq(&a->numer, &b->numer)) {
        if (a->sign) {
            return bi_lt(&a->denom, &b->denom);
        }
        else {
            return bi_lt(&b->denom, &a->denom);
        }
    }
    bigint_t l = bi_mul(&a->numer, &b->denom),
        r = bi_mul(&b->numer, &a->denom);
    int res = bi_lt(&l, &r);
    bi_free(&l);
    bi_free(&r);
    return res;
}


number_t number_add(number_t* a, number_t* b);
number_t number_sub(number_t* a, number_t* b);
number_t number_mul(number_t* a, number_t* b);
number_t number_div(number_t* a, number_t* b);
number_t number_mod(number_t* a, number_t* b);


int
print_number_frac(number_t* x) {
    int i, printed_byte_count = 0;
    if (x->flag & NUMBER_FLAG_NAN) {
        printf("NaN");
        return 3;
    }
    if (x->flag & NUMBER_FLAG_INF) {
        printf("inf");
        return 3;
    }
    if (x->flag & NUMBER_FLAG_NINF) {
        printf("-inf");
        return 4;
    }
    putchar('(');
    if (x->sign) {
        putchar('-');
        printed_byte_count++;
    }
    printed_byte_count += print_bigint_dec(&x->numer);
    printf(", ");
    print_bigint_dec(&x->denom);
    printed_byte_count += putchar(')');
    return printed_byte_count + 4;
}

int
print_number_dec(number_t* x, int round);

number_t
number_from_str(const char* str) {
    number_t n = ZERO_NUMBER();
    size_t str_length = strlen(str);
    u32 i, j, dot_pos = str_length, is_neg = 0;
    
    if (str[0] == '-') {
        str_length--;
        str++;
        is_neg = 1;
    }
    if (str_length > 2 && str[0] == 0) {
        if (str[1] == 'b' || str[1] == 'x') {
            n.numer = bigint_from_str(str);
            n.denom = ONE_BIGINT();
            n.sign = is_neg;
            return n;
        }
        else {
            printf("number_from_str: end format\n");
            return NAN_NUMBER();
        }
    }

    char* str_no_dot = (char*) malloc(str_length + 1);
    i = j = 0;
    for (i = 0; i < str_length; i++) {
        if (str[i] != '.') {
            str_no_dot[j] = str[i];
            j++;
        }
        else {
            dot_pos = i;
        }
    }
    str_no_dot[j] = '\0';

    n.sign = is_neg;
    n.numer = bigint_from_str(str_no_dot);
    n.denom = bigint_from_tens_power(str_length - dot_pos);
    number_normalize(&n);
    return n;
}

number_t
number_from_bigint(bigint_t* n);
