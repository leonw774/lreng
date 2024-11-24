#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"
#include "bigint.h"

#define BIPTR_IS_ZERO(x) (x->size == 0)

int
bit_length(unsigned long x) {
#if (defined(__clang__) || defined(__GNUC__))
    if (x != 0) {
        /* __builtin_clzl() is available since GCC 3.4.
           Undefined behavior for x == 0. */
        return (int) sizeof(unsigned long) * 8 - __builtin_clzl(x);
    }
    else {
        return 0;
    }
#elif
    const int BIT_LENGTH_TABLE[32] = {
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
    };
    int msb = 0;
    while (x >= 32) {
        msb += 6;
        x >>= 6;
    }
    msb += BIT_LENGTH_TABLE[x];
    return msb;
#endif
}

bigint_t
ONE_BIGINT() {
    bigint_t x;
    x.nan = 0;
    x.size = 1;
    x.sign = 0;
    x.digit = (u32*) malloc(sizeof(u32));
    x.digit[0] = 1;
    return x;
}

void
new_bi(bigint_t* x, u32 size) {
    /* p must be freed */
    x->nan = 0;
    x->size = size;
    x->sign = 0;
    if (size != 0) {
        x->digit = (u32*) calloc(size, sizeof(u32));
        memset(x->digit, 0, size * sizeof(u32));
    }
    else {
        x->digit = NULL;
    }
}

/* this function do new and move
   dst must be freed, src must not be freed */
void
copy_bi(bigint_t* dst, const bigint_t* src) {
    if (src->nan) {
        *dst = *src; // just copy
        dst->digit = NULL;
        return;
    }
    if (BIPTR_IS_ZERO(src)) {
        dst->nan = dst->size = dst->sign = 0;
        dst->digit = NULL;
        return;
    }
    dst->nan = 0;
    dst->size = src->size;
    dst->sign = src->sign;
    dst->digit = calloc(src->size, sizeof(u32));
    memcpy(dst->digit, src->digit, src->size * sizeof(u32));
}

void
free_bi(bigint_t* x) {
    if (x->digit != NULL) {
        free(x->digit);
        x->digit = NULL;
    }
    x->nan = x->size = x->sign = 0;
}

/* remove leading zeros by reduce size, no reallocation */
void
bi_normalize(bigint_t* x) {
    if (x->digit == NULL) {
        return;
    }
    int i = x->size - 1;
    while(i != 0 && x->digit[i] == 0) {
        i--;
    }
    /* if all is zero, set size and sign to zero */
    if (i == 0 && x->digit[0] == 0) {
        x->size = x->sign = 0;
        free(x->digit);
        x->digit = NULL;
    }
    else {
        x->size = i + 1;
    }
}

void
bi_extend(bigint_t* x, u32 added_size) {
    if (added_size == 0) {
        return;
    }
    u32 new_size = x->size + added_size;
    u32* tmp_mem = calloc(new_size, sizeof(u32));
    if (x->digit) {
        memcpy(tmp_mem, x->digit, x->size * sizeof(u32));
        free(x->digit);
    }
    x->digit = tmp_mem;
    x->size = new_size;
}

/* bigint shift left n bits for 1 <= n <= BASE_SHIFT
   return 0 means this function did not do antthing */
int
bi_shl(bigint_t* x, u32 n) {
    if (BIPTR_IS_ZERO(x) || n == 0 || n > BASE_SHIFT) {
        return 0;
    }
    u32 i, new_digit, carry = 0;
    for (i = 0; i < x->size; i++) {
        new_digit = ((x->digit[i] << n) & DIGIT_MASK) + carry;
        carry = x->digit[i] >> (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    if (carry) {
        bi_extend(x, 1);
        x->digit[x->size - 1] = carry;
    }
    return 1;
}

/* bigint shift left n bits for 1 <= n <= BASE_SHIFT
   return 0 means this function did not do antthing */
int
bi_shr(bigint_t* x, u32 n) {
    if (BIPTR_IS_ZERO(x) || n == 0 || n > BASE_SHIFT) {
        return 0;
    }
    int i;
    u32 new_digit, carry = 0;
    for (i = x->size - 1; i >= 0; i--) {
        new_digit = (carry & DIGIT_MASK) + (x->digit[i] >> n);
        carry = x->digit[i] << (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    bi_normalize(x);
    return 1;
}

int
bi_eq(bigint_t* a, bigint_t* b) {
    if (a->nan || b->nan || a->digit == NULL || b->digit == NULL
        || a->size != b->size || a->sign != b->sign 
        || memcmp(a->digit, b->digit, a->size * sizeof(u32))) {
        return 0;
    }
    return 1;
}

int
bi_lt(bigint_t* a, bigint_t* b) {
    if (a->sign == b->sign) {
        if (a->sign == 0) {
            if (a->size < b->size) {
                return 1;
            }
            else if (a->size == b->size) {
                return a->size == 0
                    || a->digit[a->size-1] < b->digit[b->size-1];
            }
            else {
                return 0;
            }
        }
        else {
            if (a->size > b->size) {
                return 1;
            }
            else if (a->size == b->size) {
                return a->size == 0
                    || a->digit[a->size-1] > b->digit[b->size-1];
            }
            else {
                return 0;
            }
        }
    }
    else {
        return a->sign == 1;
    }   
}

/* res must be freed, a and must not be freed */
void
bi_uadd(bigint_t* res, bigint_t* a, bigint_t* b) {
    u32 i, carry = 0, a_size = a->size, b_size = b->size;
    /* handle one digit */
    if (a->size == 1 && b->size == 1) {
        u32 carry = a->digit[0] + b->digit[0];
        if (carry & CARRY_MASK) {
            new_bi(res, 2);
            res->digit[0] = carry & DIGIT_MASK;
            res->digit[1] = 1;
        }
        else {
            new_bi(res, 1);
            res->digit[0] = carry;
        }
        return;
    }
    /* ensure a's size is larger */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        u32 tmp_size = a_size; a_size = b_size; b_size = tmp_size;
    }
    new_bi(res, a_size + 1);
    for (i = 0; i < b_size; ++i) {
        carry += a->digit[i] + b->digit[i];
        res->digit[i] = carry & DIGIT_MASK;
        carry >>= BASE_SHIFT;
    }
    for (; i < a_size; ++i) {
        carry += a->digit[i];
        res->digit[i] = carry & DIGIT_MASK;
        carry >>= BASE_SHIFT;
    }
    res->digit[i] = carry;
    bi_normalize(res);
}

/* res must be freed, a and must not be freed */
void
bi_usub(bigint_t* res, bigint_t* a, bigint_t* b) {
    u32 a_size = a->size, b_size = b->size;
    u32 sign = 0, borrow;
    i32 i;
    /* handle one digit */
    if (a->size == 1 && b->size == 1) {
        new_bi(res, 1);
        if (a->digit[0] > b->digit[0]) {
            res->digit[0] = a->digit[0] - b->digit[0];
        }
        else {
            res->digit[0] = b->digit[0] - a->digit[0];
            res->sign = 1;
        }
        return;
    }
    /* ensure that a > b */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        u32 tmp_size = a_size; a_size = b_size; b_size = tmp_size;
        sign = 1;
    }
    if (a_size == b_size) {
        i = a_size - 1;
        while (i >= 0 && a->digit[i] == b->digit[i]) {
            i--;
        }
        if (i == -1) {
            *res = ZERO_BIGINT();
        }
        if (a->digit[i] < b->digit[i]) {
            bigint_t* tmp = a; a = b; b = tmp;
            sign = 1;
        }
        a_size = b_size = i + 1;
    }
    new_bi(res, a_size);
    borrow = 0;
    for (i = 0; i < b_size; ++i) {
        borrow = a->digit[i] - b->digit[i] - borrow;
        res->digit[i] = borrow & DIGIT_MASK;
        borrow >>= BASE_SHIFT;
    }
    for (; i < a_size; ++i) {
        borrow = a->digit[i] - borrow;
        res->digit[i] = borrow & DIGIT_MASK;
        borrow >>= BASE_SHIFT;
    }
    if (borrow != 0) {
        printf("bi_usub: last borrow is not zero\n");
        print_bi_dec(a); puts("");
        print_bi_dec(b); puts("");
        res->nan = 1;
    }
    res->sign = sign;
    bi_normalize(res);
}

/* _u and _v must not be freed or zero (handled in bi_div or bi_mod)
    q and r must be freed */
void
bi_udivmod(bigint_t* _u, bigint_t* _v, bigint_t* q, bigint_t* r) {
    bigint_t u, v;
    u32 *uj, *v0;
    u32 u_size = _u->size, v_size = _v->size, sign = 0;
    u32 d, j, k, qj, rj, ujn, ujnm1, ujnm2;
    i32 borrow;
    u64 utop2, vnm1, vnm2;
    i64 borrow64;

    /* if u < v, no need to compute */
    if (u_size < v_size) {
        *q = ZERO_BIGINT();
        copy_bi(r, _u);
        return;
    }
    if (u_size == v_size) {
        int i = u_size - 1;
        while (i >= 0 && _u->digit[i] == _v->digit[i]) {
            i--;
        }
        if (i == -1) {
            *q = ONE_BIGINT();
            *r = ZERO_BIGINT();
            return;
        }
        if (_u->digit[i] < _v->digit[i]) {
            *q = ZERO_BIGINT();
            copy_bi(r, _u);
            return;
        }
    }
    /* if size == 1 */
    if (u_size == 1 && v_size == 1) {
        *q = ONE_BIGINT();
        *r = ONE_BIGINT();
        q->digit[0] = _u->digit[0] / _v->digit[0];
        r->digit[0] = _u->digit[0] % _v->digit[0];
        bi_normalize(q);
        bi_normalize(r);
        return;
    }
    /* explanation
        algorithm from Donald Knuth's 'Art of Computer Programming, Volume 2, 
        Section 4.3.1 Algorithm D

        given nonnegative integers _u (m+n digits) and _v (n digits), we form
        quotent q (m+1 digits) and remainder r = u mod v (n digits)

        this algorithm is long division with fast estimation of each digits. in
        long division, where dividing _u (m+n) digit by _v (n) is performed as
        dividing first n+1 digit of _u with n digits of v m times.

        a good estimation of u/v when u has n+1 digits and v has n digits is 
        q' = min(floor((u[n] * b + u[n-1]) / v[n-1]), b-1). we have q' >= q
        because

          q' * v[n-1] >= u[n] * b + u[n-1]
          q' * v[n-1] * b^(n-1) >= u
          q' >= u / (v[n-1] * b^(n-1))
             >= u / v
             >= floor(u / v) = q

        q' is good enough because

          q' <= (u[n] * b + u[n-1]) / v[n-1]
              = (u[n] * b^n + u[n-1]) * b^(n-1) / (v[n-1] * b^(n-1))
             <= u / (v[n-1] * b^(n-1))
              < u / (v - b^(n-1))

        and with q > u / v - 1, we have:

          q' - q < u / (v - b^(n-1)) - (u / v - 1)
                 = (uv - (uv - u * b^(n-1))) / (v * (v - b^(n-1))) + 1
                 = u * b^(n-1) / v / (v - b^(n-1)) + 1
                 = u / v * (b^(n-1) / (v - b^(n-1))) + 1
                 = u / v / v[n-1] + 1

          q' - q - 1 < (u / v) / v[n-1] < (q + 1) / v[n-1]

          (q' - q - 1) * v[n-1] < (q + 1) <= b

          q' - q < b / v[n-1] + 1

        . we see that the difference of q' and q is decided by v[n-1]. if we
        want q' - q < 3, we need b / v[n-1] + 1 < 3, which means v[n-1] > b / 2.
        this is what the normalization step does.
        
        to eliminate the case that q' = q + 2, we need the second significant
        digit of u and v. using:

          r' = (u[n] * b + u[n-1]) - (q' * v[n-1])
        
        , there is that:

          u - q'v <= u - q' * v[n-1] * b^(n-1) - ... - v[0]
                  <= u - q' * v[n-1] * b^(n-1) - q' * v[n-2] * b^(n-2)
                  <= u - q' * v[n-1] * b^(n-1) - q' * v[n-2] * b^(n-2)
                   < u[n] * b^[n] + u[n-1] * b^(n-1) + (u[n-2] + 1) * b^(n-2)
                     - q' * v[n-1] * b^(n-1) - q' * v[n-2] * b^(n-2)
                  == (r' * b + (u[n-2] + 1) - q' * v[n2]) * b^(n-2)
        
        . so when q' * v[n-2] > r' * b + u[n-2], u - q'v < 0, therefore q' > q
        and we can safely substract q' with 1 to eliminate the q' = q + 2 case.
    */

    /* 1. normalize:
       shift u and v so that the top digit of v >= floor(base / 2) and increase
       the size of u by one */
    d = BASE_SHIFT - bit_length(_v->digit[_v->size-1]);
    copy_bi(&u, _u);
    copy_bi(&v, _v);

    bi_extend(&u, 1);
    u_size++;
    if (d != 0) {
        bi_shl(&u, d);
        bi_shl(&v, d);
    }
    // printf("u "); print_bi(&u); puts("");
    // printf("v "); print_bi(&v); puts("");

    /* 2. initialize j:
       now u has at most m+n+1 digits and v has n digits, the quotent will
       have at most m+1 digits. in the loop, we get quotent's digits one by one
       from j = m to 0, by performing u[j:j+n] / v[0:n-1] (long division) */
    j = u_size - v_size - 1;
    new_bi(q, j+1);
    v0 = v.digit;
    vnm1 = (u64) v.digit[v_size-1];
    vnm2 = (u64) (v_size > 1) ? v.digit[v_size-2] : 0;
    /* for j = m to 0 */
    for (uj = u.digit + j; uj >= u.digit; uj--, j--) {
        ujn = uj[v_size];
        ujnm1 = uj[v_size-1];
        ujnm2 = (v_size > 1) ? uj[v_size-2] : 0;
        // printf("ujn %d ujnm1 %d\n", ujn, ujnm1);
        /* 3. Estimate qj and rj:
           this step guarentee that q[j] <= qj <= q[j] + 1 */
        utop2 = (((u64) ujn) << BASE_SHIFT) | (u64) ujnm1;
        // printf("utop2 %lld vnm1 %lld\n", utop2, vnm1);
        qj = (u32) (utop2 / vnm1);
        rj = (u32) (utop2 % vnm1);
        while (qj == DIGIT_BASE
            || (u64) qj * vnm2 > ((u64) rj << BASE_SHIFT) + ujnm2) {
            qj--;
            rj += vnm1;
            if (rj < DIGIT_BASE) {
                break;
            }
        }
        // printf("qj rj %lld %lld \n", qj, rj);

        /* 4. Substract u[j:j+n] by qj * v[0:n-1]
           because qj could be larger byone, the borrow need to be signed to
           detect that */
        borrow = 0;
        borrow64 = 0;
        for (k = 0; k < v_size; k++) {
            /* need to use 64 bit because qj * v[k] has 64 bit result */
            borrow64 = (i64) uj[k] + borrow - (i64) qj * v0[k];
            uj[k] = (u32) borrow64 & DIGIT_MASK;
            /* preform arithmetic right shift and cast to 32 bit */
            borrow = (i32) (borrow64 >> BASE_SHIFT);
        }
        uj[v_size] += borrow;

        /* 5. if the result of last step is negative i.e. borrow + u[j+n] < 0,
           decrease qj by 1 and add v[0:n-1] back to u[j:j+n] */
        if (((i32) uj[v_size]) < 0) {
            // printf("uj[v_size]) < 0\n");
            u32 carry = 0;
            /* add v[0:n-1] back to u[j:j+n] */
            for (k = 0; k < v_size; k++) {
                carry += uj[k] + v0[k];
                uj[k] = carry & DIGIT_MASK;
                carry >>= BASE_SHIFT;
            }
            qj--;
        }

        /* 6. store and quotent digit qj into q */
        q->digit[j] = qj;
        // printf("%d ", j); print_bi(q); puts("");
    }

    /* the content of u (shifted _u) is now shifted remainder, shift it back */  
    copy_bi(r, &u);
    bi_shr(r, d);
    // printf("r "); print_bi(r); puts("");
    /* normalize the result */
    bi_normalize(r);
    bi_normalize(q);
}


bigint_t
bi_add(bigint_t* a, bigint_t* b) {
    bigint_t c; 
    /* handle zeros */
    if (BIPTR_IS_ZERO(a) && BIPTR_IS_ZERO(b)) {
        return ZERO_BIGINT();
    }
    else if (BIPTR_IS_ZERO(a)) {
        copy_bi(&c, b);
        return c;
    }
    else if (BIPTR_IS_ZERO(b)) {
        copy_bi(&c, a);
        return c;
    }
    c = ZERO_BIGINT();
    if (a->sign) {
        if (b->sign) {
            bi_uadd(&c, a, b);
            c.sign = 1;
        }
        else {
            bi_usub(&c, b, a);
        }
    } else {
        if (b->sign) {
            bi_usub(&c, a, b);
        }
        else {
            bi_uadd(&c, a, b);
        }
    }
    return c;
}

bigint_t
bi_sub(bigint_t* a, bigint_t* b) {
    bigint_t c;
    /* handle zeros */
    if (BIPTR_IS_ZERO(a) && BIPTR_IS_ZERO(b)) {
        return ZERO_BIGINT();
    }
    else if (BIPTR_IS_ZERO(a)) {
        copy_bi(&c, b);
        c.sign = 1;
        return c;
    }
    else if (BIPTR_IS_ZERO(b)) {
        copy_bi(&c, a);
        return c;
    }

    if (a->sign) {
        if (b->sign) {
            bi_usub(&c, a, b);
            c.sign = !c.sign;
        }
        else {
            bi_uadd(&c, b, a);
            c.sign = !c.sign;
        }
    }
    else {
        if (b->sign) {
            bi_uadd(&c, a, b);
        }
        else {
            bi_usub(&c, a, b);
        }
    }
    return c;
}

bigint_t bi_mul(bigint_t* a, bigint_t* b) {
    bigint_t m;
    u64 i64m, a0, carry = 0;
    u32 i, a_size = a->size, b_size = b->size;
    
    /* base conditions */
    if (BIPTR_IS_ZERO(a) || BIPTR_IS_ZERO(b)) {
        return ZERO_BIGINT();
    }
    if (a->size == 1 && a->digit[0] == 1) {
        copy_bi(&m, b);
        return m;
    }
    if (b->size == 1 && b->digit[0] == 1) {
        copy_bi(&m, a);
        return m;
    }
    if (a->size == 1 && b->size == 1) {
        i64m = (u64) a->digit[0] * b->digit[0];
        new_bi(&m, 2);
        m.digit[0] = (u32) i64m & DIGIT_MASK;
        m.digit[1] = (u32) (i64m >> BASE_SHIFT) & DIGIT_MASK;
        bi_normalize(&m);
        return m;
    }
    if (a_size == 1 || b_size == 1) {
        /* ensure a_size <= b_size */
        if (a_size > b_size) {
            bigint_t* tmp;
            u32 tmp_size;
            tmp = a; a = b; b = tmp;
            tmp_size = a_size; a_size = b_size; b_size = tmp_size;
        }
        new_bi(&m, b_size + 1);
        a0 = a->digit[0];
        carry = 0;
        for (i = 0; i < b_size; i++) {
            carry += a0 * b->digit[i];
            m.digit[i] = carry & DIGIT_MASK;
            carry = carry >> BASE_SHIFT;
        }
        m.digit[i] = carry;
        bi_normalize(&m);
        return m;
    }

    /* karatsuba algorithm */
    bigint_t a_high, a_low, b_high, b_low, z0, z1, z2, tmp1, tmp2;
    u32 low_size;
    u32* tmp_mem;

    low_size = ((a_size > b_size) ? a_size : b_size) / 2;
    /* split a and b */
    if (a_size <= low_size) {
        copy_bi(&a_low, a);
        a_high = ZERO_BIGINT();
    }
    else {
        new_bi(&a_low, low_size);
        memcpy(a_low.digit, a->digit, low_size * sizeof(u32));
        new_bi(&a_high, a_size - low_size);
        memcpy(
            a_high.digit,
            a->digit + low_size,
            (a_size - low_size) * sizeof(u32)
        );
    }
    bi_normalize(&a_low);
    bi_normalize(&a_high);
    if (b_size <= low_size) {
        copy_bi(&b_low, b);
        b_high = ZERO_BIGINT();
    }
    else {
        new_bi(&b_low, low_size);
        memcpy(b_low.digit, b->digit, low_size * sizeof(u32));
        new_bi(&b_high, b_size - low_size);
        memcpy(
            b_high.digit,
            b->digit + low_size,
            (b_size - low_size) * sizeof(u32)
        );
    }
    bi_normalize(&b_low);
    bi_normalize(&b_high);

    /* z0 */
    z0 = bi_mul(&a_low, &b_low);

    /* z1' */
    tmp1 = bi_add(&a_low, &a_high);
    tmp2 = bi_add(&b_low, &b_high);
    z1 = bi_mul(&tmp1, &tmp2);
    free_bi(&tmp1);
    free_bi(&tmp2);
    free_bi(&a_low);
    free_bi(&b_low);
    
    /* z2' */
    z2 = bi_mul(&a_high, &b_high);
    free_bi(&a_high);
    free_bi(&b_high);

    /* z1 = (z1' - z2' - z0) * b^split_size */
    tmp1 = bi_sub(&z1, &z0);
    free_bi(&z1);
    z1 = bi_sub(&tmp1, &z2);
    free_bi(&tmp1);
    tmp_mem = calloc(z1.size + low_size, sizeof(u32));
    memcpy(tmp_mem + low_size, z1.digit, z1.size * sizeof(u32));
    free(z1.digit);
    z1.digit = tmp_mem;
    z1.size = z1.size + low_size;

    /* z2 = z2' * b^(2*split_size) */
    if (z2.size != 0) {
        tmp_mem = calloc(z2.size + low_size * 2, sizeof(u32));
        memcpy(tmp_mem + (low_size * 2), z2.digit, z2.size * sizeof(u32));
        free(z2.digit);
        z2.digit = tmp_mem;
        z2.size = z2.size + (low_size * 2);
    }

    /* m = z0 + z1 + z2 */
    tmp1 = bi_add(&z0, &z1);
    m = bi_add(&tmp1, &z2);
    free_bi(&z0);
    free_bi(&z1);
    free_bi(&z2);
    free_bi(&tmp1);
    if (a->sign != b->sign) {
        m.sign = 1;
    }
    return m;
}


bigint_t bi_div(bigint_t* a, bigint_t* b) {
    if (BIPTR_IS_ZERO(b)) {
        printf("bi_div: divided by zero\n");
        return NAN_BIGINT();
    }
    if (BIPTR_IS_ZERO(a)) {
        return ZERO_BIGINT();
    }
    /* if b == 1, return a */
    if (b->size == 1 && b->digit[0] == 1) {
        return *a;
    }
    /* a == b, return 1 */
    if (bi_eq(a, b)) {
        return ONE_BIGINT();
    }
    /* if a < b, return 0 */
    if (a->size == b->size && a->digit[a->size -1] < b->digit[b->size -1]
        || a->size < b->size) {
        return ZERO_BIGINT();
    }
    bigint_t q = ZERO_BIGINT(), r = ZERO_BIGINT();
    bi_udivmod(a, b, &q, &r);
    if (a->sign == b->sign) {
        free_bi(&r);
        return q;
    }
    else {
        free_bi(&q);
        return r;
    }
}

bigint_t
bi_mod(bigint_t* a, bigint_t* b) {
    bigint_t q = ZERO_BIGINT(), r = ZERO_BIGINT(), _r = ZERO_BIGINT();
    if (BIPTR_IS_ZERO(b)) {
        printf("bi_mod: divided by zero\n");
        return NAN_BIGINT();
    }
    if (BIPTR_IS_ZERO(a)) {
        return ZERO_BIGINT();
    }
    /* if b == 1, return 0 */
    if (b->size == 1 && b->digit[0] == 1) {
        return ZERO_BIGINT();
    }
    /* a == b, return 0 */
    if (bi_eq(a, b)) {
        return ZERO_BIGINT();
    }
    /* if a < b, return a */
    if (a->size == b->size && a->digit[a->size -1] < b->digit[b->size -1]
        || a->size < b->size) {
        copy_bi(&r, a);
        return r;
    }

    bi_udivmod(a, b, &q, &r);
    if (a->sign) {
        if (b->sign) {
            r.sign = 1;
            free_bi(&q);
            return r;
        }
        else {
            bi_usub(&_r, &r, b);
            free_bi(&q);
            free_bi(&r);
            return _r;
        }
    }
    else {
        if (b->sign) {
            bi_usub(&_r, &r, b);
            _r.sign = 1;
            free_bi(&q);
            free_bi(&r);
            return _r;
        }
        else {
            bi_udivmod(a, b, &q, &r);
            free_bi(&q);
            return r;
        }
    }
}


int
print_bi(bigint_t* x) {
    int i, printed_bytes_count = 0;
    if (x->nan) {
        printed_bytes_count = printf("[BiInt] NaN");
        return printed_bytes_count;
    }
    printed_bytes_count = printf(
        "[BigInt] sign=%d, size=%d, digit=",
        x->sign, x->size
    );
    fflush(stdout);
    if (BIPTR_IS_ZERO(x)) {
        return printed_bytes_count;
    }
    for (i = 0; i< x->size; i++) {
        printed_bytes_count += printf("%8x, ", x->digit[i]);
        fflush(stdout);
    }
    return printed_bytes_count;
}

/* returned dynarr contains does not always contains terminating NULL */
dynarr_t
bi_to_dec_str(bigint_t* x) {
    dynarr_t string;
    char buf[24];
    char nan[4] = "NaN";
    int i, figure_num = 0;
    string = new_dynarr(1);
    if (x->nan) {
        for (i = 0; i < 3; i++) {
            append(&string, &nan[i]);
        }
        return string;
    }
    if (x->sign) {
        append(&string, "-");
    }
    if (BIPTR_IS_ZERO(x)) {
        append(&string, "0");
    }
    else {
        if (x->size == 1) {
            figure_num = sprintf(buf, "%d", x->digit[0]);
            for (i = 0; i < figure_num; i++) {
                append(&string, &buf[i]);
            }
            return string;
        }
        else if (x->size == 2) {
            figure_num = sprintf(
                buf,
                "%lld",
                (((u64) x->digit[1]) << BASE_SHIFT) | (u64) x->digit[0]
            );
            for (i = 0; i < figure_num; i++) {
                append(&string, &buf[i]);
            }
            return string;
        }
        bigint_t y, ten, q = ZERO_BIGINT(), r = ZERO_BIGINT();
        dynarr_t reversed_digits = new_dynarr(1);
        char d;
        copy_bi(&y, x);
        new_bi(&ten, 1);
        ten.digit[0] = 10;
        while (y.size != 0) {
            bi_udivmod(&y, &ten, &q, &r);
            d = (r.digit ? r.digit[0] : 0) + '0';
            append(&reversed_digits, &d);
            free_bi(&y);
            free_bi(&r);
            copy_bi(&y, &q);
            free_bi(&q);
        }
        for (i = reversed_digits.size - 1; i >= 0; i--) {
            append(&string, &((char*) (reversed_digits.data))[i]);
        }
        free_bi(&y);
        free_bi(&ten);
        free_bi(&q);
        free_bi(&r);
    }
    return string;
}

int
print_bi_dec(bigint_t* x) {
    int printed_bytes_count = 0;
    printed_bytes_count += printf("");
    dynarr_t x_str = bi_to_dec_str(x);
    char* x_cstr = to_str(&x_str);
    printed_bytes_count = printf("[BigInt] %s", x_cstr);
    free(x_cstr);
    free_dynarr(&x_str);
    return printed_bytes_count;
}

/* expect string of decimal, heximal, or binary u32eger */
bigint_t
bi_from_str(const char* str) {
    bigint_t x, t1, t2;
    size_t str_length = strlen(str), base = 10;
    /* bin & hex can determine needed size quickly */
    u32 i, carry = 0, j = 0, d, safe_size, is_neg = 0;

    if (str[0] == '-') {
        str++;
        str_length--;
        is_neg = 1;
    }
    if (str[0] == '0') {
        if (str_length == 1) {
            return ZERO_BIGINT();
        }
        if (str[1] == 'x') {
            safe_size = (str_length - 2) * 4 / 31 + 1;
            base = 16;
        }
        else if (str[1] == 'b') {
            safe_size = (str_length - 2) / 31 + 1;
            base = 2;
        }
        str += 2;
        str_length -= 2;
    }

    if (base == 2) {
        new_bi(&x, safe_size);
        /* set it bit by bit */
        for (i = 0, j = str_length - 1; i < str_length; i++, j--) {
            if (str[i] == '1') {
                x.digit[j / 31] |= (1 << (j % 31));
            }
        }
    }
    else {
        new_bi(&x, (base == 10) ? 1 : safe_size);
        for (i = 0; i < str_length; i++) {
            /* mul base */
            if (i != 0) {
                if (base == 10) {
                    /* x * 10 = (x << 2 + x) << 1 */
                    copy_bi(&t1, &x);
                    bi_shl(&t1, 2);
                    bi_uadd(&t2, &t1, &x);
                    bi_shl(&t2, 1);
                    free_bi(&x);
                    free_bi(&t1);
                    copy_bi(&x, &t2);
                    free_bi(&t2);
                }
                else {
                    /* base == 16 */
                    bi_shl(&x, 4);
                }
            }

            /* add string digit */
            d = str[i];
            if ('0' <= d && d <= '9') {
                d -= '0';
            }
            else if ('a' <= d && d <= 'f') {
                d -= 'a' - 10;
            }
            else if ('A' <= d && d <= 'F') {
                d -= 'A' - 10;
            }
            carry += x.digit[j] + d;
            x.digit[j] = carry & DIGIT_MASK;
            carry >>= BASE_SHIFT;
            if (carry) {
                j++;
                bi_extend(&x, 1);
            }
            /* print_bigint_dec(&x); puts(""); */
        }
        if (carry != 0) {
            x.digit[j] += carry;
        }
        bi_normalize(&x);
    }
    if (is_neg) {
        x.sign = 1;
    }
    return x;
}

bigint_t
bi_from_tens_power(i32 exp) {
    if (exp < 0) {
        printf("bi_from_tens_power: negative exp\n");
        return NAN_BIGINT();
    }
    bigint_t x = ONE_BIGINT(), t1, t2, t3;
    while (exp != 0) {
        if (exp == 1) {
            /* x *= 10 --> x = ((x << 2) + x) << 1 */
            copy_bi(&t1, &x);
            bi_shl(&t1, 2);
            bi_uadd(&t2, &t1, &x);
            bi_shl(&t2, 1);
            free_bi(&x);
            free_bi(&t1);
            x = t2;
            exp--;
        }
        else if (exp == 2) {
            /* x *= 100 --> x = ((x << 4) + (x << 3) + x) << 2 */
            copy_bi(&t1, &x);
            copy_bi(&t2, &x);
            bi_shl(&t1, 4);
            bi_shl(&t2, 3);
            bi_uadd(&t3, &t1, &t2);
            free_bi(&t1);
            bi_uadd(&t1, &t3, &x);
            bi_shl(&t1, 2);
            free_bi(&x);
            free_bi(&t2);
            free_bi(&t3);
            x = t1;
            exp -= 2;
        }
        else {
            /* x *= 1000 --> x = ((x << 10) - (x << 4) - (x << 3)) */
            copy_bi(&t1, &x);
            copy_bi(&t2, &x);
            bi_shl(&t1, 10);
            bi_shl(&t2, 4);
            bi_usub(&t3, &t1, &t2);
            free_bi(&t1);
            bi_shl(&x, 3);
            free_bi(&t2);
            bi_usub(&t2, &t3, &x);
            free_bi(&x);
            free_bi(&t1);
            free_bi(&t3);
            copy_bi(&x, &t2);
            exp -= 3;
        }
        /* print_bigint_dec(&x); puts(""); */
    }
    return x;
}
