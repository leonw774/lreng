#include "arena.h"
#include "my_arenas.h"
#include "bigint.h"
#include "dynarr.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* some part of these codes are taken or rewriten from CPython */

#define BIPTR_IS_ZERO(x) (x->size == 0)

static inline int
bit_length(unsigned long x)
{
#if (defined(__clang__) || defined(__GNUC__))
    if (x != 0) {
        /* __builtin_clzl() is available since GCC 3.4.
           Undefined behavior for x == 0. */
        return (int)sizeof(unsigned long) * 8 - __builtin_clzl(x);
    } else {
        return 0;
    }
#elif
    const int BIT_LENGTH_TABLE[32]
        = { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 };
    int msb = 0;
    while (x >= 32) {
        msb += 6;
        x >>= 6;
    }
    msb += BIT_LENGTH_TABLE[x];
    return msb;
#endif
}

static const u32 STATIC_BYTES[257] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255, 256,
};

#define BYTE_BIGINT_MAX 256

/* all 1 ~ BYTE_BIGINT_MAX is shared */
inline bigint_t
BYTE_BIGINT(u32 b)
{
    assert(0 < b && b <= BYTE_BIGINT_MAX);
    return (bigint_t) {
        .sign = 0,
        .nan = 0,
        .size = 1,
        .shared = 1,
        .digit = (u32*)&STATIC_BYTES[b],
    };
}

inline void
bi_new(bigint_t* x, u32 size)
{
#ifdef ENABLE_DEBUG_LOG
    assert(x->digit == NULL);
#endif
    x->shared = 0;
    x->nan = 0;
    x->size = size;
    x->sign = 0;
    if (size != 0) {
        x->digit = (u32*)calloc(size, sizeof(u32));
        memset(x->digit, 0, size * sizeof(u32));
    } else {
        x->digit = NULL;
    }
}

/* this function do new and move */
inline void
bi_copy(bigint_t* dst, const bigint_t* src)
{
#ifdef ENABLE_DEBUG_LOG
    assert(dst->digit == NULL);
#endif
    if (src->nan) {
        *dst = *src; /* shallow copy */
        dst->digit = NULL;
        return;
    }
    if (src->shared) {
        *dst = *src; /* shallow copy */
        return;
    }
    if (BIPTR_IS_ZERO(src)) {
        dst->nan = dst->size = dst->sign = 0;
        dst->digit = NULL;
        return;
    }
    *dst = *src;
    dst->digit = malloc(src->size * sizeof(u32));
    memcpy(dst->digit, src->digit, src->size * sizeof(u32));
}

inline void
bi_free(bigint_t* x)
{
    if (!x->shared && x->digit != NULL) {
        free(x->digit);
    }
    x->digit = NULL;
    x->shared = x->nan = x->size = x->sign = 0;
}

/* remove leading zeros by reduce size, no reallocation */
static inline void
bi_normalize(bigint_t* x)
{
    if (x->digit == NULL) {
        return;
    }
    int i = x->size - 1;
    while (i != 0 && x->digit[i] == 0) {
        i--;
    }
    /* if all is zero, free it and become zero */
    if (i == 0 && x->digit[0] == 0) {
        bi_free(x);
    }
    /* use byte number if possible */
    else if (i == 0 && x->sign == 0 && x->digit[0] <= BYTE_BIGINT_MAX) {
        u32 tmp = x->digit[0];
        bi_free(x);
        *x = BYTE_BIGINT(tmp);
    } else {
        x->size = i + 1;
    }
}

static inline void
bi_extend(bigint_t* x, u32 added_size)
{
    if (added_size == 0) {
        return;
    }
    if (x->shared) {
        u32 tmp = x->digit[0];
        x->digit = (u32*)malloc(sizeof(u32));
        x->digit[0] = tmp;
        x->shared = 0;
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

/* bigint shift left n bits for 1 <= n <= BASE_SHIFT */
static inline void
bi_shl(bigint_t* x, u32 n)
{
    if (BIPTR_IS_ZERO(x) || n == 0 || n > BASE_SHIFT) {
        return;
    }
    u32 i, new_digit, carry = 0;
    u8 is_shared = x->shared;
    if (x->shared) {
        u32 tmp = x->digit[0];
        x->digit = (u32*)malloc(sizeof(u32));
        x->digit[0] = tmp;
        x->shared = 0;
    }
    for (i = 0; i < x->size; i++) {
        new_digit = ((x->digit[i] << n) & DIGIT_MASK) + carry;
        carry = x->digit[i] >> (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    if (carry) {
        bi_extend(x, 1);
        x->digit[x->size - 1] = carry;
    }
    if (is_shared && x->size == 1 && x->digit[0] <= BYTE_BIGINT_MAX) {
        u32 tmp = x->digit[0];
        bi_free(x);
        *x = BYTE_BIGINT(tmp);
    }
}

/* bigint shift right n bits for 1 <= n <= BASE_SHIFT */
static inline void
bi_shr(bigint_t* x, u32 n)
{
    if (BIPTR_IS_ZERO(x) || n == 0 || n > BASE_SHIFT) {
        return;
    }
    if (x->shared) {
        u32 tmp = x->digit[0];
        x->digit = (u32*)malloc(sizeof(u32));
        x->digit[0] = tmp;
        x->shared = 0;
    }
    int i;
    u32 new_digit, carry = 0;
    for (i = x->size - 1; i >= 0; i--) {
        new_digit = (carry & DIGIT_MASK) + (x->digit[i] >> n);
        carry = x->digit[i] << (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    bi_normalize(x);
}

inline int
bi_eq(bigint_t* a, bigint_t* b)
{
    if (a->nan || b->nan || a->digit == NULL || b->digit == NULL
        || a->size != b->size || a->sign != b->sign
        || memcmp(a->digit, b->digit, a->size * sizeof(u32))) {
        return 0;
    }
    return 1;
}

inline int
bi_lt(bigint_t* a, bigint_t* b)
{
    if (a->sign == b->sign) {
        if (a->sign == 0) {
            if (a->size < b->size) {
                return 1;
            } else if (a->size == b->size) {
                return a->size == 0
                    || a->digit[a->size - 1] < b->digit[b->size - 1];
            } else {
                return 0;
            }
        } else {
            if (a->size > b->size) {
                return 1;
            } else if (a->size == b->size) {
                return a->size == 0
                    || a->digit[a->size - 1] > b->digit[b->size - 1];
            } else {
                return 0;
            }
        }
    } else {
        return a->sign == 1;
    }
}

void
bi_uadd(bigint_t* res, bigint_t* a, bigint_t* b)
{
    u32 i, carry = 0, a_size = a->size, b_size = b->size;
    /* handle one digit */
    if (a->size == 1 && b->size == 1) {
        u32 carry = a->digit[0] + b->digit[0];
        if (carry & CARRY_MASK) {
            bi_new(res, 2);
            res->digit[0] = carry & DIGIT_MASK;
            res->digit[1] = 1;
        } else {
            bi_new(res, 1);
            res->digit[0] = carry;
        }
        bi_normalize(res);
        return;
    }
    /* ensure a's size is larger */
    if (a_size < b_size) {
        bigint_t* tmp = a;
        a = b;
        b = tmp;
        u32 tmp_size = a_size;
        a_size = b_size;
        b_size = tmp_size;
    }
    bi_new(res, a_size + 1);
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

void
bi_usub(bigint_t* res, bigint_t* a, bigint_t* b)
{
    u32 a_size = a->size, b_size = b->size, sign = 0, borrow;
    i64 i;
    /* handle one digit */
    if (a->size == 1 && b->size == 1) {
        bi_new(res, 1);
        if (a->digit[0] >= b->digit[0]) {
            res->digit[0] = a->digit[0] - b->digit[0];
        } else {
            res->digit[0] = b->digit[0] - a->digit[0];
            res->sign = 1;
        }
        bi_normalize(res);
        return;
    }
    /* ensure that a > b */
    if (a_size < b_size) {
        u32 tmp_size = a_size;
        bigint_t* tmp = a;
        a = b;
        b = tmp;
        a_size = b_size;
        b_size = tmp_size;
        sign = 1;
    }
    if (a_size == b_size) {
        i = (i64)a_size - 1;
        while (i >= 0 && a->digit[i] == b->digit[i]) {
            i--;
        }
        if (i == -1) {
            *res = ZERO_BIGINT;
        }
        if (a->digit[i] < b->digit[i]) {
            bigint_t* tmp = a;
            a = b;
            b = tmp;
            sign = 1;
        }
        a_size = b_size = i + 1;
    }
    bi_new(res, a_size);
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
        print_bi_dec(a, '\n');
        print_bi_dec(b, '\n');
        res->nan = 1;
    }
    res->sign = sign;
    bi_normalize(res);
}

void
bi_umul(bigint_t* res, bigint_t* a, bigint_t* b)
{
    u64 i64m, a0, carry = 0;
    u32 i, a_size = a->size, b_size = b->size;
    /* base conditions */
    if (BIPTR_IS_ZERO(a) || BIPTR_IS_ZERO(b)) {
        *res = ZERO_BIGINT;
        return;
    }
    if (a_size == 1 && a->digit[0] == 1) {
        bi_copy(res, b);
        return;
    }
    if (b_size == 1 && b->digit[0] == 1) {
        bi_copy(res, a);
        return;
    }
    if (a_size == 1 && b_size == 1) {
        i64m = (u64)a->digit[0] * b->digit[0];
        bi_new(res, 2);
        res->digit[0] = (u32)i64m & DIGIT_MASK;
        res->digit[1] = (u32)(i64m >> BASE_SHIFT) & DIGIT_MASK;
        bi_normalize(res);
        return;
    }
    if (a_size == 1 || b_size == 1) {
        /* ensure a_size <= b_size */
        if (a_size > b_size) {
            bigint_t* tmp;
            u32 tmp_size;
            tmp = a;
            a = b;
            b = tmp;
            tmp_size = a_size;
            a_size = b_size;
            b_size = tmp_size;
        }
        bi_new(res, b_size + 1);
        a0 = a->digit[0];
        carry = 0;
        for (i = 0; i < b_size; i++) {
            carry += a0 * b->digit[i];
            res->digit[i] = carry & DIGIT_MASK;
            carry = carry >> BASE_SHIFT;
        }
        res->digit[i] = carry;
        bi_normalize(res);
        return;
    }

    /* karatsuba algorithm */
    bigint_t a_high, a_low, b_high, b_low, z0, z1, z2, tmp1, tmp2;
    u32 low_size;
    u32* tmp_mem;

    /* init */
    a_high = a_low = b_high = b_low = ZERO_BIGINT;

    low_size = ((a_size > b_size) ? a_size : b_size) / 2;
    /* split a and b */
    if (a_size <= low_size) {
        bi_copy(&a_low, a);
        a_high = ZERO_BIGINT;
    } else {
        bi_new(&a_low, low_size);
        memcpy(a_low.digit, a->digit, low_size * sizeof(u32));
        bi_new(&a_high, a_size - low_size);
        memcpy(
            a_high.digit, a->digit + low_size, (a_size - low_size) * sizeof(u32)
        );
    }
    bi_normalize(&a_low);
    bi_normalize(&a_high);
    if (b_size <= low_size) {
        bi_copy(&b_low, b);
        b_high = ZERO_BIGINT;
    } else {
        bi_new(&b_low, low_size);
        memcpy(b_low.digit, b->digit, low_size * sizeof(u32));
        bi_new(&b_high, b_size - low_size);
        memcpy(
            b_high.digit, b->digit + low_size, (b_size - low_size) * sizeof(u32)
        );
    }
    bi_normalize(&b_low);
    bi_normalize(&b_high);

    /* z0 */
    bi_umul(&z0, &a_low, &b_low);

    /* z1' */
    tmp1 = bi_add(&a_low, &a_high);
    tmp2 = bi_add(&b_low, &b_high);
    bi_umul(&z1, &tmp1, &tmp2);
    bi_free(&tmp1);
    bi_free(&tmp2);
    bi_free(&a_low);
    bi_free(&b_low);

    /* z2' */
    bi_umul(&z2, &a_high, &b_high);
    bi_free(&a_high);
    bi_free(&b_high);

    /* z1 = (z1' - z2' - z0) * b^split_size */
    tmp1 = bi_sub(&z1, &z0);
    bi_free(&z1);
    z1 = bi_sub(&tmp1, &z2);
    bi_free(&tmp1);
    tmp_mem = calloc(z1.size + low_size, sizeof(u32));
    memcpy(tmp_mem + low_size, z1.digit, z1.size * sizeof(u32));
    if (z1.shared) {
        z1.digit = tmp_mem;
        z1.shared = 0;
    } else {
        free(z1.digit);
        z1.digit = tmp_mem;
    }
    z1.size = z1.size + low_size;

    /* z2 = z2' * b^(2*split_size) */
    if (z2.size != 0) {
        tmp_mem = calloc(z2.size + low_size * 2, sizeof(u32));
        memcpy(tmp_mem + (low_size * 2), z2.digit, z2.size * sizeof(u32));
        if (z2.shared) {
            z2.digit = tmp_mem;
            z2.shared = 0;
        } else {
            free(z2.digit);
            z2.digit = tmp_mem;
        }
        z2.size = z2.size + (low_size * 2);
    }

    /* m = z0 + z1 + z2 */
    tmp1 = bi_add(&z0, &z1);
    *res = bi_add(&tmp1, &z2);
    bi_free(&z0);
    bi_free(&z1);
    bi_free(&z2);
    bi_free(&tmp1);
}

void
bi_udivmod(bigint_t* _u, bigint_t* _v, bigint_t* q, bigint_t* r)
{
    bigint_t u = ZERO_BIGINT, v = ZERO_BIGINT;
    u32 *uj, *v0;
    u32 u_size = _u->size, v_size = _v->size;
    u32 d, j, k, qj, rj, ujn, ujnm1, ujnm2;
    i32 borrow;
    u64 utop2, vnm1, vnm2;
    i64 borrow64;

    /* if u < v, no need to compute */
    if (u_size < v_size) {
        *q = ZERO_BIGINT;
        bi_copy(r, _u);
        return;
    }
    if (u_size == v_size) {
        int i = u_size - 1;
        while (i >= 0 && _u->digit[i] == _v->digit[i]) {
            i--;
        }
        if (i == -1) {
            *q = BYTE_BIGINT(1);
            *r = ZERO_BIGINT;
            return;
        }
        if (_u->digit[i] < _v->digit[i]) {
            *q = ZERO_BIGINT;
            bi_copy(r, _u);
            return;
        }
    }
    /* if size == 1 */
    if (u_size == 1 && v_size == 1) {
        bi_new(q, 1);
        bi_new(r, 1);
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
       the size of u by one
    */
    d = BASE_SHIFT - bit_length(_v->digit[_v->size - 1]);
    bi_copy(&u, _u);
    bi_copy(&v, _v);

    bi_extend(&u, 1);
    u_size++;
    if (d != 0) {
        bi_shl(&u, d);
        bi_shl(&v, d);
    }
    /*
    printf("u "); print_bi(&u, '\n');
    printf("v "); print_bi(&v, '\n');
    */

    /* 2. initialize j:
       now u has at most m+n+1 digits and v has n digits, the quotent will
       have at most m+1 digits. in the loop, we get quotent's digits one by one
       from j = m to 0, by performing u[j:j+n] / v[0:n-1] (long division)
    */
    j = u_size - v_size - 1;
    bi_new(q, j + 1);
    v0 = v.digit;
    vnm1 = (u64)v.digit[v_size - 1];
    vnm2 = (u64)(v_size > 1) ? v.digit[v_size - 2] : 0;
    /* for j = m to 0 */
    for (uj = u.digit + j; uj >= u.digit; uj--, j--) {
        ujn = uj[v_size];
        ujnm1 = uj[v_size - 1];
        ujnm2 = (v_size > 1) ? uj[v_size - 2] : 0;
        /* printf("ujn %d ujnm1 %d\n", ujn, ujnm1); */

        /* 3. Estimate qj and rj:
           this step guarentee that q[j] <= qj <= q[j] + 1
        */
        utop2 = (((u64)ujn) << BASE_SHIFT) | (u64)ujnm1;
        /* printf("utop2 %lld vnm1 %lld\n", utop2, vnm1); */
        qj = (u32)(utop2 / vnm1);
        rj = (u32)(utop2 % vnm1);
        while (qj == DIGIT_BASE
               || (u64)qj * vnm2 > ((u64)rj << BASE_SHIFT) + ujnm2) {
            qj--;
            rj += vnm1;
            if (rj < DIGIT_BASE) {
                break;
            }
        }
        /* printf("qj rj %lld %lld \n", qj, rj); */

        /* 4. Substract u[j:j+n] by qj * v[0:n-1]
           because qj could be larger byone, the borrow need to be signed to
           detect that
        */
        borrow = 0;
        borrow64 = 0;
        for (k = 0; k < v_size; k++) {
            /* need to use 64 bit because qj * v[k] has 64 bit result */
            borrow64 = (i64)uj[k] + borrow - (i64)qj * v0[k];
            uj[k] = (u32)borrow64 & DIGIT_MASK;
            /* preform arithmetic right shift and cast to 32 bit */
            borrow = (i32)(borrow64 >> BASE_SHIFT);
        }
        uj[v_size] += borrow;

        /* 5. if the result of last step is negative i.e. borrow + u[j+n] < 0,
           decrease qj by 1 and add v[0:n-1] back to u[j:j+n]
        */
        if (((i32)uj[v_size]) < 0) {
            /* printf("uj[v_size]) < 0\n"); */
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
        /* printf("%d ", j); print_bi(q, '\n'); */
    }

    /* the content of u (shifted _u) is now shifted remainder, shift it back */
    bi_copy(r, &u);
    bi_shr(r, d);
    /* normalize the result */
    bi_normalize(r);
    bi_normalize(q);
    /* free u & v */
    bi_free(&u);
    bi_free(&v);
    /*
    printf("r "); print_bi(r, '\n');
    printf("q "); print_bi(r, '\n');
    */
}

inline bigint_t
bi_add(bigint_t* a, bigint_t* b)
{
    bigint_t c = ZERO_BIGINT;
    /* handle zeros */
    if (BIPTR_IS_ZERO(a) && BIPTR_IS_ZERO(b)) {
        return ZERO_BIGINT;
    } else if (BIPTR_IS_ZERO(a)) {
        bi_copy(&c, b);
        return c;
    } else if (BIPTR_IS_ZERO(b)) {
        bi_copy(&c, a);
        return c;
    }
    c = ZERO_BIGINT;
    if (a->sign) {
        if (b->sign) {
            bi_uadd(&c, a, b);
            c.sign = 1;
        } else {
            bi_usub(&c, b, a);
        }
    } else {
        if (b->sign) {
            bi_usub(&c, a, b);
        } else {
            bi_uadd(&c, a, b);
        }
    }
    return c;
}

inline bigint_t
bi_sub(bigint_t* a, bigint_t* b)
{
    bigint_t c = ZERO_BIGINT;
    /* handle zeros */
    if (BIPTR_IS_ZERO(a) && BIPTR_IS_ZERO(b)) {
        return ZERO_BIGINT;
    } else if (BIPTR_IS_ZERO(a)) {
        bi_copy(&c, b);
        c.sign = 1;
        return c;
    } else if (BIPTR_IS_ZERO(b)) {
        bi_copy(&c, a);
        return c;
    }

    if (a->sign) {
        if (b->sign) {
            bi_usub(&c, a, b);
            c.sign = !c.sign;
        } else {
            bi_uadd(&c, b, a);
            c.sign = !c.sign;
        }
    } else {
        if (b->sign) {
            bi_uadd(&c, a, b);
        } else {
            bi_usub(&c, a, b);
        }
    }
    return c;
}

inline bigint_t
bi_mul(bigint_t* a, bigint_t* b)
{
    bigint_t m = ZERO_BIGINT;
    bi_umul(&m, a, b);
    if (a->sign != b->sign) {
        m.sign = 1;
    }
    return m;
}

inline bigint_t
bi_div(bigint_t* a, bigint_t* b)
{
    bigint_t q = ZERO_BIGINT, r = ZERO_BIGINT;
    if (BIPTR_IS_ZERO(b)) {
        printf("bi_div: divided by zero\n");
        return NAN_BIGINT();
    }
    if (BIPTR_IS_ZERO(a)) {
        return ZERO_BIGINT;
    }
    /* if |b| == 1, just copy a */
    if (b->size == 1 && b->digit[0] == 1) {
        bi_copy(&q, a);
        if (a->sign != b->sign) {
            q.sign = 1;
        }
        return q;
    }
    /* a == b, return 1 */
    if (bi_eq(a, b)) {
        return BYTE_BIGINT(1);
    }
    /* if |a| < |b|, return 0 */
    int a_less_than_b = a->size == b->size
        && (a->digit[a->size - 1] < b->digit[b->size - 1] || a->size < b->size);
    if (a_less_than_b) {
        return ZERO_BIGINT;
    }
    bi_udivmod(a, b, &q, &r);
    bi_free(&r);
    if (a->sign != b->sign) {
        q.sign = 1;
    }
    return q;
}

inline bigint_t
bi_mod(bigint_t* a, bigint_t* b)
{
    bigint_t q = ZERO_BIGINT, r = ZERO_BIGINT, _r = ZERO_BIGINT;
    if (BIPTR_IS_ZERO(b)) {
        printf("bi_mod: divided by zero\n");
        return NAN_BIGINT();
    }
    if (BIPTR_IS_ZERO(a)) {
        return ZERO_BIGINT;
    }
    /* if |b| == 1, return 0 */
    if (b->size == 1 && b->digit[0] == 1) {
        return ZERO_BIGINT;
    }
    /* a == b, return 0 */
    if (bi_eq(a, b)) {
        return ZERO_BIGINT;
    }
    /* if |a| < |b|, return a */
    int a_less_than_b = a->size == b->size
        && (a->digit[a->size - 1] < b->digit[b->size - 1] || a->size < b->size);
    if (a_less_than_b) {
        bi_copy(&r, a);
        return r;
    }

    bi_udivmod(a, b, &q, &r);
    bi_free(&q);
    if (BIPTR_IS_ZERO((&r))) {
        return ZERO_BIGINT;
    }
    if (a->sign != b->sign) {
        bi_usub(&_r, b, &r);
        bi_free(&r);
        r = _r;
    }
    if (b->sign) {
        r.sign = 1;
    }
    return r;
}

inline int
print_bi(bigint_t* x, char end)
{
    int i, printed_bytes_count = 0;
    if (x->nan) {
        printed_bytes_count = printf("[BiInt] NaN");
        return printed_bytes_count;
    }
    printed_bytes_count
        = printf("[BigInt] sign=%d, size=%d, digit=", x->sign, x->size);
    fflush(stdout);
    if (BIPTR_IS_ZERO(x)) {
        return printed_bytes_count;
    }
    for (i = 0; i < x->size; i++) {
        printed_bytes_count += printf("%8x, ", x->digit[i]);
        fflush(stdout);
    }
    if (end != '\0') {
        printed_bytes_count += printf("%c", end);
    }
    return printed_bytes_count;
}

/* returned dynarr of char does not contains terminating character NULL */
dynarr_t
bi_to_dec_str(const bigint_t* x)
{
    dynarr_t string;
    char buf[24];
    char nan[4] = "NaN";
    int i, figure_num = 0;
    string = dynarr_new(1);
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
        append(&string, "ZERO");
    } else {
        if (x->size == 1) {
            figure_num = sprintf(buf, "%d", x->digit[0]);
            for (i = 0; i < figure_num; i++) {
                append(&string, &buf[i]);
            }
            return string;
        } else if (x->size == 2) {
            figure_num = sprintf(
                buf,
                "%llu",
                (((unsigned long long)x->digit[1]) << BASE_SHIFT) | x->digit[0]
            );
            for (i = 0; i < figure_num; i++) {
                append(&string, &buf[i]);
            }
            return string;
        } else {
            bigint_t y = ZERO_BIGINT, q = ZERO_BIGINT, r = ZERO_BIGINT;
            bigint_t ten = BYTE_BIGINT(10);
            dynarr_t reversed_digits = dynarr_new(1);
            char d;
            bi_copy(&y, x);
            while (y.size != 0) {
                bi_udivmod(&y, &ten, &q, &r);
                d = (r.digit ? r.digit[0] : 0) + '0';
                append(&reversed_digits, &d);
                bi_free(&y);
                bi_free(&r);
                bi_copy(&y, &q);
                bi_free(&q);
            }
            for (i = reversed_digits.size - 1; i >= 0; i--) {
                append(&string, at(&reversed_digits, i));
            }
            dynarr_free(&reversed_digits);
            bi_free(&y);
            /* bi_free(&ten); */
            bi_free(&q);
            bi_free(&r);
        }
    }
    return string;
}

inline int
print_bi_dec(const bigint_t* x, char end)
{
    int printed_bytes_count = 0;
    dynarr_t x_str = bi_to_dec_str(x);
    char* x_cstr = to_str(&x_str);
    printed_bytes_count = printf("[BigInt] %s", x_cstr ? x_cstr : "(null)");
    if (end != '\0') {
        printed_bytes_count += printf("%c", end);
    }
    free(x_cstr);
    dynarr_free(&x_str);
    return printed_bytes_count;
}

/* expect string of decimal, heximal, or binary u32eger */
bigint_t
bi_from_str(const char* str)
{
    bigint_t x = ZERO_BIGINT;
    size_t str_length = strlen(str), base = 10;
    u32 i = 0, j = 0, sign = 0, safe_size = 0;

    if (str[0] == '0' && str_length == 1) {
        return ZERO_BIGINT;
    }
    /* before doing real decoding, use sscanf to check the str is presenting
       number between 1~BYTE_BIGINT_MAX
    */
    if (str_length <= 3) {
        u32 n;
        if (str[1] == 'x') {
            sscanf(str + 2, "%x", &n);
            if (n == 1 && n <= BYTE_BIGINT_MAX) {
                return BYTE_BIGINT(n);
            }
        } else if (str[1] == 'b') {
            sscanf(str + 2, "%x", &n);
            if (n == 1 && n <= BYTE_BIGINT_MAX) {
                return BYTE_BIGINT(n);
            }
        } else {
            sscanf(str, "%u", &n);
            if (n == 1 && n <= BYTE_BIGINT_MAX) {
                return BYTE_BIGINT(n);
            }
        }
    }

    if (str[0] == '-') {
        str++;
        str_length--;
        sign = 1;
    }
    if (str[0] == '0') {
        /*
        if (str_length == 1) {
            return ZERO_BIGINT;
        }
        */
        /* bin & hex can determine needed size quickly */
        if (str[1] == 'x') {
            safe_size = (str_length - 2) * 4 / 31 + 1;
            base = 16;
        } else if (str[1] == 'b') {
            safe_size = (str_length - 2) / 31 + 1;
            base = 2;
        }
        str += 2;
        str_length -= 2;
    }

    if (base == 2) {
        bi_new(&x, safe_size);
        /* set it bit by bit */
        for (i = 0, j = str_length - 1; i < str_length; i++, j--) {
            if (str[i] == '1') {
                x.digit[j / 31] |= (1 << (j % 31));
            }
        }
    } else {
        bigint_t t1 = ZERO_BIGINT, t2 = ZERO_BIGINT;
        bi_new(&x, (base == 10) ? 1 : safe_size);
        for (i = 0; i < str_length; i++) {
            /* x *= base */
            if (i != 0) {
                if (base == 10) {
                    /* x * 10 = (x << 2 + x) << 1 */
                    bi_copy(&t1, &x);
                    bi_shl(&t1, 2);
                    bi_uadd(&t2, &t1, &x);
                    bi_shl(&t2, 1);
                    bi_free(&x);
                    bi_free(&t1);
                    x = t2;
                    t2.digit = NULL;
                } else {
                    /* base == 16 */
                    bi_shl(&x, 4);
                }
            }
            /* add string digit */
            {
                u32 d = str[i];
                if ('0' <= d && d <= '9') {
                    d -= '0';
                } else if ('a' <= d && d <= 'f') {
                    d -= 'a' - 10;
                } else if ('A' <= d && d <= 'F') {
                    d -= 'A' - 10;
                } else {
                    printf("bi_from_str: invalid digit\n");
                    x.nan = 1;
                    break;
                }
                if (d != 0) {
                    t2 = BYTE_BIGINT(d);
                    bi_uadd(&t1, &x, &t2);
                    bi_free(&x);
                    bi_free(&t2);
                    x = t1;
                    t1.digit = NULL;
                }
            }
            /*
            printf("%d\n", d);
            print_bi(&x, '\n');
            print_bi_dec(&x, '\n');
            */
        }
        bi_free(&t1);
        bi_free(&t2);
        bi_normalize(&x);
    }
    x.sign = sign;
    return x;
}

bigint_t
bi_from_tens_power(i32 exp)
{
    bigint_t x = BYTE_BIGINT(1), t1, t2, t3;
    if (exp < 0) {
        printf("bi_from_tens_power: negative exp\n");
        return NAN_BIGINT();
    }
    if (exp == 0) {
        return BYTE_BIGINT(1);
    }
    if (exp == 1) {
        return BYTE_BIGINT(10);
    }
    if (exp == 2) {
        return BYTE_BIGINT(100);
    }
    t1 = t2 = t3 = ZERO_BIGINT;
    while (exp != 0) {
        if (exp == 1) {
            /* x *= 10 --> x = ((x << 2) + x) << 1 */
            bi_copy(&t1, &x);
            bi_shl(&t1, 2);
            bi_uadd(&t2, &t1, &x);
            bi_shl(&t2, 1);
            bi_free(&x);
            bi_free(&t1);
            x = t2;
            t2.digit = NULL;
            exp--;
        } else if (exp == 2) {
            /* x *= 100 --> x = ((x << 4) + (x << 3) + x) << 2 */
            bi_copy(&t1, &x);
            bi_copy(&t2, &x);
            bi_shl(&t1, 4);
            bi_shl(&t2, 3);
            bi_uadd(&t3, &t1, &t2);
            bi_free(&t1);
            bi_uadd(&t1, &t3, &x);
            bi_shl(&t1, 2);
            bi_free(&x);
            bi_free(&t2);
            bi_free(&t3);
            x = t1;
            t1.digit = NULL;
            exp -= 2;
        } else {
            /* x *= 1000 --> x = ((x << 10) - (x << 4) - (x << 3)) */
            bi_copy(&t1, &x);
            bi_copy(&t2, &x);
            bi_shl(&t1, 10);
            bi_shl(&t2, 4);
            bi_usub(&t3, &t1, &t2);
            bi_free(&t1);
            bi_shl(&x, 3);
            bi_free(&t2);
            bi_usub(&t2, &t3, &x);
            bi_free(&x);
            bi_free(&t1);
            bi_free(&t3);
            x = t2;
            t2.digit = NULL;
            exp -= 3;
        }
        /* print_bigint_dec(&x, '\n'); */
    }
    return x;
}
