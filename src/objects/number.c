#include "number.h"
#include "dynarr.h"
#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline void
number_copy(number_t* dst, const number_t* src)
{
    if (src->numer.nan) {
        dst->numer = NAN_BIGINT();
        dst->denom = NAN_BIGINT();
    } else if (src->numer.size == 0) {
        dst->numer = ZERO_BIGINT;
        dst->denom = BYTE_BIGINT(1);
    }
    bi_copy(&dst->numer, &src->numer);
    bi_copy(&dst->denom, &src->denom);
}

inline void
number_free(number_t* x)
{
    bi_free(&x->numer);
    bi_free(&x->denom);
}

void
number_normalize(number_t* x)
{
    int sign = 0;
    bigint_t a = ZERO_BIGINT, b = ZERO_BIGINT, t1 = ZERO_BIGINT,
             t2 = ZERO_BIGINT, one = BYTE_BIGINT(1);

    /* flags */
    if (x->numer.nan || x->denom.nan) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        x->numer = NAN_BIGINT();
        x->denom = NAN_BIGINT();
    }

    /* special cases */
    /* n = 0 */
    if (x->numer.size == 0) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        *x = ZERO_NUMBER;
        return;
    }
    /* n == 1 or d = 1 */
    if (bi_eq(&x->numer, &one) || bi_eq(&x->denom, &one)) {
        return;
    }
    /* d == 0 */
    if (x->denom.size == 0) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        *x = NAN_NUMBER;
        return;
    }
    /* n == d */
    if (bi_eq(&x->numer, &x->denom)) {
        bi_free(&x->numer);
        bi_free(&x->denom);
        x->numer = BYTE_BIGINT(1);
        x->denom = BYTE_BIGINT(1);
        return;
    }

    /* normalize the sign */
    if (x->numer.sign != x->denom.sign) {
        sign = 1;
    }
    x->numer.sign = x->denom.sign = 0;

    /* euclidian algorithm */
    bi_copy(&a, &x->numer);
    bi_copy(&b, &x->denom);
    /* printf("a "); print_bi(&a, '\n');
    printf("b "); print_bi(&b, '\n'); */
    while (b.size != 0) {
        bi_free(&t2);
        t2 = bi_mod(&a, &b); /* t2 = a mod b */
        bi_free(&a);
        bi_copy(&a, &b); /* a = t1 */
        bi_free(&b);
        bi_copy(&b, &t2); /* b = t2 */
        /* printf("a "); print_bi(&a, '\n');
        printf("b "); print_bi(&b, '\n'); */
    }

    /* a is gcd of numer & denom */
    if (!bi_eq(&a, &one)) {
        bi_free(&t1);
        t1 = bi_div(&x->numer, &a);
        bi_free(&x->numer);
        x->numer = t1;
        bi_free(&t2);
        t2 = bi_div(&x->denom, &a);
        bi_free(&x->denom);
        x->denom = t2;
        /* dont need to free t1 and t2 because they are own by x now */
    } else {
        bi_free(&t1);
        bi_free(&t2);
    }
    bi_free(&a);
    bi_free(&b);
    x->numer.sign = sign;
}

inline int
number_eq(number_t* a, number_t* b)
{
    /*  nan != anything */
    if (a->numer.nan || b->numer.nan) {
        return 0;
    }
    /* a == b == 0 */
    if (a->numer.size == 0 && b->numer.size == 0) {
        return 1;
    }
    return bi_eq(&a->numer, &b->numer) && bi_eq(&a->denom, &b->denom);
}

inline int
number_lt(number_t* a, number_t* b)
{
    /* if one of them is nan: always false */
    if (a->numer.nan || b->numer.nan) {
        return 0;
    }
    /* obvious cases */
    if ((a->numer.sign != b->numer.sign)) {
        return a->numer.sign;
    }

    if (bi_eq(&a->denom, &b->denom)) {
        return bi_lt(&a->numer, &b->numer);
    }
    if (bi_eq(&a->numer, &b->numer)) {
        return bi_lt(&b->denom, &a->denom);
    }
    bigint_t l = bi_mul(&a->numer, &b->denom), r = bi_mul(&b->numer, &a->denom);
    int res = bi_lt(&l, &r);
    bi_free(&l);
    bi_free(&r);
    return res;
}

inline number_t
number_add(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER;
    bigint_t t1 = ZERO_BIGINT, t2 = ZERO_BIGINT;
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0 && b->numer.size == 0) {
        return ZERO_NUMBER;
    }
    if (a->numer.size == 0) {
        number_copy(&res, b);
        return res;
    }
    if (b->numer.size == 0) {
        number_copy(&res, a);
        return res;
    }
    t1 = bi_mul(&a->numer, &b->denom);
    t2 = bi_mul(&b->numer, &a->denom);
    res.numer = bi_add(&t1, &t2);
    res.denom = bi_mul(&a->denom, &b->denom);
    bi_free(&t1);
    bi_free(&t2);
    number_normalize(&res);
    return res;
}

inline number_t
number_sub(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER;
    bigint_t t1 = ZERO_BIGINT, t2 = ZERO_BIGINT;
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0 && b->numer.size == 0) {
        return ZERO_NUMBER;
    }
    if (a->numer.size == 0) {
        number_copy(&res, b);
        res.numer.sign = !res.numer.sign;
        return res;
    }
    if (b->numer.size == 0) {
        number_copy(&res, a);
        return res;
    }
    t1 = bi_mul(&a->numer, &b->denom);
    t2 = bi_mul(&b->numer, &a->denom);
    res.numer = bi_sub(&t1, &t2);
    res.denom = bi_mul(&a->denom, &b->denom);
    bi_free(&t1);
    bi_free(&t2);
    number_normalize(&res);
    return res;
}

inline number_t
number_mul(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER;
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0 || b->numer.size == 0) {
        return ZERO_NUMBER;
    }
    res.numer = bi_mul(&a->numer, &b->numer);
    res.denom = bi_mul(&a->denom, &b->denom);
    number_normalize(&res);
    return res;
}

inline number_t
number_div(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER;
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0) {
        return ZERO_NUMBER;
    }
    if (b->numer.size == 0) {
        return NAN_NUMBER;
    }
    res.numer = bi_mul(&a->numer, &b->denom);
    res.denom = bi_mul(&a->denom, &b->numer);
    number_normalize(&res);
    return res;
}

inline number_t
number_mod(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER;
    bigint_t t1, t2;
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    t1 = bi_mul(&a->numer, &b->denom);
    t2 = bi_mul(&b->numer, &a->denom);
    res.numer = bi_mod(&t1, &t2);
    res.denom = bi_mul(&a->denom, &b->denom);
    bi_free(&t1);
    bi_free(&t2);
    number_normalize(&res);
    return res;
}

/* the exponent b can only be integer because
   rational exponent can result in irrational number */
number_t
number_exp(number_t* a, number_t* b)
{
    number_t res = EMPTY_NUMBER, cur = EMPTY_NUMBER, t1 = EMPTY_NUMBER;
    bigint_t e = ZERO_BIGINT, q = ZERO_BIGINT, r = ZERO_BIGINT;
    bigint_t two = BYTE_BIGINT(2);
    if (a->numer.nan || b->numer.nan) {
        return NAN_NUMBER;
    }
    if (b->denom.size != 1 || b->denom.digit[0] != 1) {
        bi_free(&two);
        return NAN_NUMBER;
    }
    if (b->numer.digit[0] == 0) {
        bi_free(&two);
        return ONE_NUMBER;
    }

    res = ONE_NUMBER; /* res = 1 */
    number_copy(&cur, a); /* cur = a */
    bi_copy(&e, &b->numer); /* e = b */
    while (e.size != 0) {
        /* r = e % 2 */
        r = bi_mod(&e, &two);
        /* e = e / 2 */
        q = bi_div(&e, &two);
        bi_free(&e);
        bi_copy(&e, &q);
        /* if r == 1:
             res = res * cur */
        if (r.size != 0) {
            t1 = number_mul(&res, &cur);
            number_free(&res);
            res = t1;
        }
        /* cur = cur * cur */
        t1 = number_mul(&cur, &cur);
        number_free(&cur);
        cur = t1;
        bi_free(&r);
        bi_free(&q);
    }
    number_free(&cur);
    bi_free(&two);
    if (b->numer.sign) {
        bigint_t tmp = res.numer;
        res.numer = res.denom;
        res.denom = tmp;
    }
    return res;
}

/*
    because bigint division is truncate division
    * for in positive is floor (1.5 -> 1)
    * for negative is ceiling (-1.5 -> -1)
*/

number_t
number_ceil(number_t* a)
{
    number_t res = EMPTY_NUMBER;
    bigint_t t1, one = BYTE_BIGINT(1);
    if (a->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0) {
        return ZERO_NUMBER;
    }
    if (a->numer.sign) {
        // bigint division is truncate division -
        res.numer = bi_div(&a->numer, &a->denom);
        res.denom = BYTE_BIGINT(1);
    } else {
        t1 = bi_div(&a->numer, &a->denom);
        res.numer = bi_add(&t1, &one);
        res.denom = one;
        bi_free(&t1);
    }
    return res;
}

number_t
number_floor(number_t* a)
{
    number_t res = EMPTY_NUMBER;
    bigint_t t1, one = BYTE_BIGINT(1);
    if (a->numer.nan) {
        return NAN_NUMBER;
    }
    if (a->numer.size == 0) {
        return ZERO_NUMBER;
    }
    if (a->numer.sign) {

        t1 = bi_div(&a->numer, &a->denom);
        res.numer = bi_sub(&t1, &one);
        res.denom = one;
        bi_free(&t1);
    } else {
        res.numer = bi_div(&a->numer, &a->denom);
        res.denom = BYTE_BIGINT(1);
    }
    return res;
}

void
print_number_struct(number_t* x)
{
    printf("[Number]");
    printf("\tnumer=");
    print_bi(&x->numer, '\n');
    printf("\tdenom=");
    print_bi(&x->denom, '\n');
}

int
number_print_frac(const number_t* x, char end)
{
    int printed_bytes_count = 0;
    printed_bytes_count += printf("[Number] ");
    if (x->numer.nan) {
        printed_bytes_count += printf("NaN");
        return printed_bytes_count;
    }
    if (x->numer.size == 0) {
        printed_bytes_count += printf("0");
        return printed_bytes_count;
    }
    putchar('(');
    printed_bytes_count += print_bi_dec(&x->numer, '\0');
    printf(", ");
    printed_bytes_count += print_bi_dec(&x->denom, '\0');
    putchar(')');
    if (end != '\0') {
        printed_bytes_count += printf("%c", end);
    }
    return printed_bytes_count + 4;
}

int
number_print_dec(const number_t* x, int precision, char end)
{
    int printed_bytes_count = 0;
    int i, n_exp, d_exp, m;
    dynarr_t res_str;
    char* res_cstr = NULL;
    char dot = '.' - '0';
    number_t _x, t = EMPTY_NUMBER, q = EMPTY_NUMBER, r = EMPTY_NUMBER,
                 ten = number_from_i32(10), ten_to_e = EMPTY_NUMBER;

    if (x->numer.nan) {
        return printf("[Number] NaN");
    }
    if (x->numer.size == 0) {
        return printf("[Number] 0");
    }

    /* find the lowest m such that b^m >= abs(n/d) */
    {
        dynarr_t n_str = bi_to_dec_str(&x->numer);
        dynarr_t d_str = bi_to_dec_str(&x->denom);
        char* n_cstr = to_str(&n_str);
        char* d_cstr = to_str(&d_str);
        n_exp = strlen(n_cstr);
        d_exp = strlen(d_cstr);
        m = n_exp - d_exp + (strcmp(n_cstr, d_cstr) >= 0);
        free(n_cstr);
        free(d_cstr);
        dynarr_free(&n_str);
        dynarr_free(&d_str);
    }
    /* round the number to 10^(m - precision):
       for each exponent i := 1 ~ precision,
         e = m - i
         q = (x - x % 10^e) / 10^e
         push q into digit stack
         x -= q * 10^e
    */
    i = 1;
    {
        bigint_t ten_to_abs_e = bi_from_tens_power((m < 0) ? -m - i : m - i);
        if (m < 0) {
            ten_to_e.numer = BYTE_BIGINT(1);
            ten_to_e.denom = ten_to_abs_e;
        } else {
            ten_to_e.numer = ten_to_abs_e;
            ten_to_e.denom = BYTE_BIGINT(1);
        }
    }
    res_str = dynarr_new(1);
    number_copy(&_x, x);
    while (1) {
        /* q = (x - x % 10^e) / 10^e */
        number_free(&r);
        number_free(&t);
        number_free(&q);
        r = number_mod(&_x, &ten_to_e);
        t = number_sub(&_x, &r);
        q = number_div(&t, &ten_to_e);
        if (q.numer.size != 1 || q.numer.digit[0] >= 10) {
            printf("number_print_dec: q's numer >= 10\n");
            exit(OTHER_ERR_CODE);
        }
        append(&res_str, &(q.numer.digit[0]));
        i++;
        if (m - i == -1) {
            append(&res_str, &dot);
        }
        if (i > precision) {
            break;
        }
        /* x -= q * 10^e */
        number_free(&r);
        r = number_mul(&q, &ten_to_e);
        number_free(&q);
        q = number_sub(&_x, &r);
        number_free(&_x);
        number_copy(&_x, &q);
        /* ten_to_e /= 10 */
        number_free(&t);
        t = number_div(&ten_to_e, &ten);
        number_free(&ten_to_e);
        number_copy(&ten_to_e, &t);
    }
    number_free(&t);
    res_cstr = to_str(&res_str);
    for (i = 0; i < res_str.size; i++) {
        res_cstr[i] += '0';
    }
    printed_bytes_count = printf("[Number] %s", res_cstr);

    free(res_cstr);
    if (end != '\0') {
        printed_bytes_count += printf("%c", end);
    }
    return printed_bytes_count;
}

number_t
number_from_str(const char* str)
{
    number_t n = EMPTY_NUMBER;
    size_t str_length = strlen(str);
    u32 i = 0, j = 0, dot_pos = str_length, is_less_one = 0;

    if (str[0] == '0') {
        if (str_length == 1) {
            return ZERO_NUMBER;
        }
        if (str[1] == 'b' || str[1] == 'x') {
            n.numer = bi_from_str(str);
            n.denom = BYTE_BIGINT(1);
            return n;
        } else if (str[1] == '.') {
            is_less_one = 1;
        } else {
            printf("number_from_str: bad format\n");
            return NAN_NUMBER;
        }
    }
    {
        char* str_no_dot = (char*)malloc(str_length + 1);
        for (i = 0; i < str_length; i++) {
            if (is_less_one && i == 0) {
                continue;
            }
            if (str[i] != '.') {
                str_no_dot[j] = str[i];
                j++;
            } else {
                dot_pos = i + 1;
            }
        }
        str_no_dot[j] = '\0';
        /* printf("%s %d %d\n", str_no_dot, dot_pos, str_length - dot_pos); */
        n.numer = bi_from_str(str_no_dot);
        n.denom = bi_from_tens_power(str_length - dot_pos);
        /* print_bi_dec(&n.numer, '\n');
        print_bi_dec(&n.denom, '\n'); */
        free(str_no_dot);
    }
    number_normalize(&n);
    return n;
}

number_t
number_from_i32(i32 i)
{
    number_t n = EMPTY_NUMBER;
    u32 j, sign = 0;
    if (i == 0) {
        return ZERO_NUMBER;
    }
    if (i == 1) {
        return ONE_NUMBER;
    }
    n.denom = BYTE_BIGINT(1);
    if (i < 0) {
        sign = 1;
        /* cast to 64-bit first to prevent overflow at -2^31 */
        j = -((i64)i);
    } else {
        j = i;
    }
    if (j >= DIGIT_BASE) {
        bi_new(&n.numer, 2);
        n.numer.digit[0] = j & DIGIT_MASK;
        n.numer.digit[1] = 1;
    } else if (sign == 0 && j <= 256) {
        n.numer = BYTE_BIGINT(j);
    } else {
        bi_new(&n.numer, 1);
        n.numer.digit[0] = j;
    }
    n.numer.sign = sign;
    return n;
}
