#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "objects.h"

int bit_length(unsigned long x) {
#if (defined(__clang__) || defined(__GNUC__))
    if (x != 0) {
        // __builtin_clzl() is available since GCC 3.4.
        // Undefined behavior for x == 0.
        return (int)sizeof(unsigned long) * 8 - __builtin_clzl(x);
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

#define BASE_SHIFT 31
#define DIGIT_BASE ((uint32_t) 1 << BASE_SHIFT)
#define CARRY_MASK ((uint32_t) 1 << BASE_SHIFT)
#define DIGIT_MASK ((uint32_t) 1 << BASE_SHIFT) - 1
#define IS_ONE_DIGIT(x) (x->size == 1 \
    && x->digit[0] < ((uint32_t) 1 << (BASE_SHIFT - 1)))
#define ZERO_BIGINT() ((bigint_t) {\
    .invalid = 0, .sign = 0, .size = 0, .digit = NULL})
#define INVALID_BIGINT() ((bigint_t) {\
    .invalid = 1, .sign = 0, .size = 0, .digit = NULL})

bigint_t ONE_BIGINT() {
    bigint_t x;
    x.invalid = 0;
    x.size = 1;
    x.sign = 0;
    x.digit = (uint32_t*) malloc(sizeof(uint32_t));
    x.digit[0] = 1;
    return x;
}

bigint_t
new_bigint(uint32_t size) {
    bigint_t x;
    x.invalid = 0;
    x.size = size;
    x.sign = 0;
    if (size != 0) {
        x.digit = (uint32_t*) malloc(size * sizeof(uint32_t));
        memset(x.digit, 0, size * sizeof(uint32_t));
    }
    else {
        x.digit = NULL;
    }
    return x;
}

bigint_t
copy_bigint(bigint_t* x) {
    bigint_t y;
    if (x->invalid) {
        y = *x;
        y.digit = NULL;
        return y;
    }
    if (x->size == 0) {
        y.invalid = y.size = y.sign = 0;
        y.digit = NULL;
        return y;
    }
    y.size = x->size;
    y.sign = x->sign;
    y.digit = malloc(x->size * sizeof(uint32_t));
    memcpy(y.digit, x->digit, x->size * sizeof(uint32_t));
    return y;
}

void
free_bigint(bigint_t* x) {
    if (x->digit != NULL) {
        free(x->digit);
        x->digit = NULL;
    }
    x->size = x->sign = 0;
}

int
print_bigint(bigint_t* x) {
    printf("[BigInt] sign=%d, size=%d digit=", x->sign, x->size);
    int i; 
    for (i = 0; i< x->size; i++) printf("%8x, ", x->digit[i]);
}

int
print_bigint_dec(bigint_t* x) {
    printf("[BigInt] decimal=");
    if (x->sign) {
        printf("-");
    }
    if (x->size == 0) {
        printf("0");
    }
    else {
        bigint_t y = copy_bigint(x), ten = new_bigint(1);
        ten.digit[0] = 10;
        int i;
        for (i = 0; i< x->size; i++) {
            bigint_t tmp = bi_mod(&y, &ten);
            printf("%c", tmp.digit[0] + '0');
            free_bigint(&y);
            y = tmp;
        }
    }
}

/* remove leading zeros by reduce size, no reallocation */
void
bi_normalize(bigint_t* x) {
    int i = x->size - 1;
    while(i != 0 && x->digit[i] == 0) {
        i--;
    }
    /* all is zero, set size and sign to zero */
    if (i == 0 && x->digit[i] == 0) {
        x->size = x->sign = 0;
    }
    else {
        x->size = i + 1;
    }
}

void
bi_extend(bigint_t* x, uint32_t added_size) {
    if (added_size == 0) {
        return;
    }
    uint32_t* tmp_mem = malloc(added_size * sizeof(uint32_t));
    memset(tmp_mem, 0, added_size);
    if (x->digit) {
        memcpy(tmp_mem, x->digit, x->size * sizeof(uint32_t));
    }
    x->digit = tmp_mem;
    x->size = added_size;
}

/* bigint shift left n bits for 1 <= n <= BASE_SHIFT */
int
bi_shl(bigint_t* x, uint32_t n) {
    if (x->size == 0 || n == 0 || n > BASE_SHIFT) {
        return 0;
    }
    uint32_t i, new_digit, carry = 0;
    for (i = 0; i < x->size; i++) {
        new_digit = ((x->digit[i] << n) & DIGIT_MASK) + carry;
        carry = x->digit[i] >> (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    if (carry) {
        bi_extend(x, 1);
        x->digit[x->size - 1] = carry;
    }
    return 0;
}

/* bigint shift left n bits for 1 <= n <= BASE_SHIFT */
int
bi_shr(bigint_t* x, uint32_t n) {
    if (x->size == 0 || n == 0 || n > BASE_SHIFT) {
        return 0;
    }
    uint32_t i, new_digit, carry = 0;
    for (i = x->size - 1; i >= 0; i--) {
        new_digit = (carry & DIGIT_MASK) + (x->digit[i] >> n);
        carry = x->digit[i] << (BASE_SHIFT - n);
        x->digit[i] = new_digit;
    }
    bi_normalize(x);
    return 0;
}

bigint_t
bi_abs_add(bigint_t* a, bigint_t* b) {
    uint32_t a_size = a->size, b_size = b->size;
    /* handle one digit */
    if (IS_ONE_DIGIT(a) && IS_ONE_DIGIT(b)) {
        uint32_t carry = a->digit[0] + b->digit[0];
        bigint_t c;
        if (carry & CARRY_MASK) {
            c = new_bigint(2);
            c.digit[0] = carry & DIGIT_MASK;
            c.digit[1] = 1;
        }
        else {
            c = new_bigint(1);
            c.digit[0] = carry;
        }
        return c;
    }
    /* ensure a's size is larger */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        uint32_t tmp_size = a_size; b_size = a_size; a_size = tmp_size;
    }
    bigint_t c = new_bigint(a_size + 1);
    uint32_t i, carry = 0;
    for (i = 0; i < b_size; ++i) {
        carry += a->digit[i] + b->digit[i];
        c.digit[i] = carry & DIGIT_MASK;
        carry >>= BASE_SHIFT;
    }
    for (; i < a_size; ++i) {
        carry += a->digit[i];
        c.digit[i] = carry & DIGIT_MASK;
        carry >>= BASE_SHIFT;
    }
    c.digit[i] = carry;
    bi_normalize(&c);
    return c;
}

bigint_t
bi_abs_sub(bigint_t* a, bigint_t* b) {
    uint32_t a_size = a->size, b_size = b->size;
    uint32_t sign = 0;
    /* handle one digit */
    if (IS_ONE_DIGIT(a) && IS_ONE_DIGIT(b)) {
        bigint_t c = new_bigint(1);
        if (a->digit[0] > b->digit[0]) {
            c.digit[0] = a->digit[0] - b->digit[0];
        }
        else {
            c.digit[0] = b->digit[0] - a->digit[0];
            c.sign = 1;
        }
        return c;
    }
    /* ensure that a > b */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        uint32_t tmp_size = a_size; b_size = a_size; a_size = tmp_size;
        uint32_t sign = 1;
    }
    if (a_size == b_size) {
        int i = a_size - 1;
        while (i >= 0 && a->digit[i] == b->digit[i]) {
            i--;
        }
        if (i == -1) {
            return ZERO_BIGINT();
        }
        if (a->digit[i] < b->digit[i]) {
            bigint_t* tmp = a; a = b; b = tmp;
            sign = 1;
        }
        a_size = b_size = i + 1;
    }

    bigint_t c = new_bigint(a_size);
    uint32_t i, borrow = 0;
    for (i = 0; i < b_size; ++i) {
        borrow = a->digit[i] - b->digit[i] - borrow;
        c.digit[i] = borrow & DIGIT_MASK;
        borrow >>= BASE_SHIFT;
    }
    for (; i < a_size; ++i) {
        borrow = a->digit[i] - borrow;
        c.digit[i] = borrow & DIGIT_MASK;
        borrow >>= BASE_SHIFT;
    }
    if (borrow != 0) {
        printf("bi_abs_sub: last borrow is not zero\n");
        c.invalid = 1;
        return c;
    }
    c.sign = sign;
    bi_normalize(&c);
    return c;
}

bigint_t
bi_add(bigint_t* a, bigint_t* b) {
    /* handle zeros */
    if (a->size == 0 && b->size == 0) {
        return ZERO_BIGINT();
    }
    else if (a->size == 0) {
        return copy_bigint(b);
    }
    else if (b->size == 0) {
        return copy_bigint(a);
    }
    bigint_t c;
    if (a->sign) {
        if (b->sign) {
            c = bi_abs_add(a, b);
            c.sign = 1;
        }
        else {
            c = bi_abs_sub(b, a);
        }
    }
    else {
        if (b->sign) {
            c = bi_abs_sub(a, b);
            c.sign = 1;
        }
        else {
            c = bi_abs_add(a, b);
            c.sign = 1;
        }
    }
    return c;
}

bigint_t
bi_sub(bigint_t* a, bigint_t* b) {
    /* handle zeros */
    if (a->size == 0 && b->size == 0) {
        return ZERO_BIGINT();
    }
    else if (a->size == 0) {
        bigint_t c = copy_bigint(b);
        c.sign = 1;
        return c;
    }
    else if (b->size == 0) {
        return copy_bigint(a);
    }
    bigint_t c;
    if (a->sign) {
        if (b->sign) {
            c = bi_abs_sub(a, b);
        }
        else {
            c = bi_abs_add(b, a);
            c.sign = !c.sign;
        }
    }
    else {
        if (b->sign) {
            c = bi_abs_sub(a, b);
            c.sign = 1;
        }
        else {
            c = bi_abs_add(a, b);
            c.sign = 1;
        }
    }
    return c;
}

// bigint_t bi_mul(bigint_t* a, bigint_t* b);

void
bi_abs_divmod(bigint_t* u, bigint_t* v, bigint_t* q, bigint_t* r) {
    /* u and v would not be zero (handled in bi_div or bi_mod)
       q and r would be renewed and are owned by caller
    */

    /* algorithm from Donald Knuth's 'Art of Computer Programming, Volume 2, 
       Section 4.3.1 Algorithm D
    
       given nonnegative integers u (m+n digits) and v (n digits), we form
       quotent q (m+1 digits) and remainder r = u mod v (n digits)
    */
    
    bigint_t u1, v1, a;
    uint32_t *uj, *v0;
    uint32_t u_size = u->size, v_size = v->size, sign = 0;
    uint32_t d, j, k, qj, rj, vnm1, vnm2;
    uint64_t utop2;

    /* ensure that q & r are zero */
    free_bigint(q);
    free_bigint(r);

    /* ensure that u > v */
    if (u_size < v_size) {
        bigint_t* tmp = u; u = v; v = tmp;
        uint32_t tmp_size = u_size; v_size = u_size; u_size = tmp_size;
        sign = 1;
    }
    else {
        *q = ZERO_BIGINT();
        *r = copy_bigint(u);
        return;
    }
    if (u_size == v_size) {
        int i = u_size - 1;
        while (i >= 0 && u->digit[i] == v->digit[i]) {
            i--;
        }
        if (i == -1) {
            *q = ONE_BIGINT();
            *r = ZERO_BIGINT();
            return;
        }
        if (u->digit[i] < v->digit[i]) {
            *q = ZERO_BIGINT();
            *r = copy_bigint(u);
            return;
        }
        u_size = v_size = i + 1;
    }

    /* 1. normalize:
       shift u and v so that the top digit of v > floor(base / 2) and increase
       the size of u by one */
    d = BASE_SHIFT - bit_length(v->digit[v->size-1]);
    u1 = copy_bigint(u);
    v1 = copy_bigint(v);
    bi_extend(&u1, 1);
    u_size += 1;
    if (d != 0) {
        bi_shl(&u1, d);
        bi_shl(&v1, d);
    }
    /* 2. initialize j:
       now u has at most m+n+1 digits and v has n digits, the quotent will
       have at most m+1 digits. in the loop, we get these digits one by one
       from j = m+1 to 0, by performing u[j:j+n-1] / v[0:n-1] */
    j = u_size - v_size;
    free_bigint(q);
    *q = new_bigint(j);
    v0 = v->digit;
    vnm1 = v->digit[v_size-1];
    vnm2 = v->digit[v_size-2];
    for (uj = u->digit + j; uj >= u->digit; uj--) {
        /* 3. Estimate qj and rj */
        utop2 = (uint64_t) uj[v_size] << BASE_SHIFT + uj[v_size-1];
        qj = (uint32_t) (utop2 / vnm1);
        rj = (uint32_t) (utop2 % vnm1);
        while (qj == DIGIT_BASE
            || qj * vnm2 > ((uint64_t) rj << BASE_SHIFT) | uj[u_size-2]) {
            qj -= 1;
            rj += vnm1;
            if (rj < DIGIT_BASE) {
                break;
            }
        }

        /* 4. Substract u[j:j+n] by qj * v[0:n-1] */
        int32_t borrow = 0;
        int64_t borrow_tmp = 0;
        for (k = 0; k < v_size; k++) {
            /* need to use 64 bit because qj * v[k] has 64 bit result */
            borrow_tmp = (int32_t) uj[k] + borrow
                - (int64_t) qj * (int64_t) v0[k];
            uj[k] = (uint32_t) borrow_tmp & DIGIT_MASK;
            /* preform arithmetic right shift in 32 bit to  */
            borrow = (int32_t) (borrow_tmp >> BASE_SHIFT);
        }

        /* 5. if the result of last step is negative i.e. borrow + u[j+n] < 0,
           decrease qj by 1. */
        if ((int32_t) uj[v_size] + borrow < 0) {
            uint32_t carry = 0;
            /* add v[0:n-1] back to u[j:j+n] */
            for (k = 0; k < v_size; k++) {
                carry += uj[k] + v0[k];
                uj[k] = carry & DIGIT_MASK;
                carry >>= BASE_SHIFT;
            }
            qj -= 1;
        }

        /* 6. store and quotent digit qj into q */
        q->digit[j] = qj;
    }

    /* the content of u1 (shifted u) is now shifted remainder, shift it back */  
    *r = copy_bigint(&u1);
    bi_shr(r, d);

    /* normalize the result */
    bi_normalize(r);
    bi_normalize(q);
}


bigint_t bi_div(bigint_t* a, bigint_t* b);

bigint_t
bi_mod(bigint_t* a, bigint_t* b) {
    if (b->sign == 1) {
        return INVALID_BIGINT();
    }
    if (a->size == 0 || a->size > b->size) {
        return ZERO_BIGINT();
    }
    if (a->size == b->size && a->digit[a->size -1] < b->digit[b->size -1]) {
        return ZERO_BIGINT();
    }
    bigint_t q;
    bigint_t tmp = copy_bigint(a);
    /* estimated quotent = (a_size - b_size) * base */
    /* tmp = a - estimated quotent * b */
    bigint_t m;
    /* m = tmp - tmp / b */
    return m;
}

bigint_t
bi_eq(bigint_t* a, bigint_t* b) {
    if (a->size != b->size || (a->sign != b->sign && a->size != 0)
        || memcmp(a->digit, b->digit, a->size)) {
        return ZERO_BIGINT();
    }
    bigint_t res = new_bigint(1);
    res.digit[0] = 1;
    return res;

}

// bigint_t bi_lt(bigint_t* a, bigint_t* b);


/* expect string of decimal, heximal, or binary uint32_teger */
bigint_t
bigint_from_str(const char* str) {
    bigint_t x, t1, t2;
    uint32_t str_length = strlen(str), base = 10;
    /* dec & hex need at least 4 bit per digit */
    uint32_t i, carry = 0, j = 0, d, safe_size = str_length * 4 / 31 + 1;

    if (str_length > 2 && str[0] == '0') {
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
        x = new_bigint(safe_size);
        /* set it bit by bit */
        for (i = str_length - 1; i >= 0; i--) {
            if (str[i] == '1') {
                uint32_t digit_index = str_length - 1 - i;
                x.digit[digit_index / 31] |= (1 << (digit_index % 31));
            }
        }
    }
    else {
        x = new_bigint(safe_size);
        for (i = 0; i < str_length; i++) {
            /* mul base */
            if (i != 0) {
                if (base == 10) {
                    /* x * 10 = (x << 2 + x) << 1 */
                    t1 = copy_bigint(&x);
                    bi_shl(&t1, 2);
                    t2 = bi_abs_add(&t1, &x);
                    bi_shl(&t2, 1);
                    free_bigint(&x);
                    free_bigint(&t1);
                    x = t2;
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
                j += 1;
            }

            printf("i=%d, '%c'\n", i, str[i]);
            print_bigint(&x);
            puts("");
        }
        bi_normalize(&x);
    }
    return x;
}

bigint_t
bigint_from_tens_power(uint32_t exp) {
    bigint_t x = ONE_BIGINT(), t1, t2, t3;
    while (exp != 0) {
        if (exp == 1) {
            /* x *= 10 --> x = ((x << 2) + x) << 1 */
            t1 = copy_bigint(&x);
            bi_shl(&t1, 2);
            t2 = bi_abs_add(&t1, &x);
            bi_shl(&t2, 1);
            free_bigint(&x);
            free_bigint(&t1);
            x = t2;
            exp -= 1;
        }
        else if (exp == 2) {
            /* x *= 100 --> x = ((x << 4) + (x << 3) + x) << 2 */
            t1 = copy_bigint(&x);
            t2 = copy_bigint(&x);
            bi_shl(&t1, 4);
            bi_shl(&t2, 3);
            t3 = bi_abs_add(&t1, &t2);
            free_bigint(&t1);
            t1 = bi_abs_add(&t3, &x);
            bi_shl(&t1, 2);
            free_bigint(&x);
            free_bigint(&t2);
            free_bigint(&t3);
            x = t1;
            exp -= 2;
        }
        else {
            /* x *= 1000 --> x = ((x << 10) - (x << 4) - (x << 3)) */
            t1 = copy_bigint(&x);
            t2 = copy_bigint(&x);
            bi_shl(&t1, 10);
            bi_shl(&t2, 4);
            t3 = bi_abs_sub(&t1, &t2);
            free_bigint(&t1);
            bi_shl(&x, 3);
            free_bigint(&t2);
            t2 = bi_abs_sub(&t3, &x);
            free_bigint(&x);
            free_bigint(&t1);
            free_bigint(&t3);
            x = t2;
            exp -= 3;
        }

        printf("exp=%d\n", exp);
        print_bigint(&x);
        puts("");
    }
    return x;
}
