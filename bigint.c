#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "objects.h"

#define CARRY_SHIFT 31
#define CARRY_MASK (uint) (1 << CARRY_SHIFT)
#define DIGIT_MASK (uint) (1 << CARRY_SHIFT) - 1
#define ZERO_BIGINT() ((bigint_t) {\
    .invalid = 0, .sign = 0, .size = 0, .digit = NULL})
#define INVALID_BIGINT() ((bigint_t) {\
    .invalid = 1, .sign = 0, .size = 0, .digit = NULL})

bigint_t ONE_BIGINT() {
    bigint_t x;
    x.invalid = 0;
    x.size = 1;
    x.sign = 0;
    x.digit = (uint*) malloc(sizeof(uint));
    x.digit[0] = 1;
    return x;
}

bigint_t
new_bigint(uint size) {
    bigint_t x;
    x.invalid = 0;
    x.size = size;
    x.sign = 0;
    x.digit = (uint*) malloc(size * sizeof(uint));
    memset(x.digit, 0, size * sizeof(uint));
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
    y.digit = malloc(x->size * sizeof(uint));
    memcpy(y.digit, x->digit, x->size * sizeof(uint));
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
bi_extend(bigint_t* x, int new_size) {
    if (new_size < x->size) {
        printf("bi_extend: Extending size is smaller than original");
        exit(OTHER_ERR_CODE);
    }
    else if (new_size == x->size) {
        return;
    }
    int* tmp_mem = malloc(new_size * sizeof(int));
    memset(tmp_mem, 0, new_size);
    if (x->digit) {
        memcpy(tmp_mem, x->digit, x->size * sizeof(int));
    }
    x->digit = tmp_mem;
    x->size = new_size;
}

/* bigint shift left n bits for 1 <= n <= CARRY_SHIFT */
int
bi_shl(bigint_t* x, uint n) {
    if (x->size == 0 || n == 0 || n > CARRY_SHIFT) {
        return 0;
    }
    uint i, new_digit, carry = 0;
    for (i = 0; i < x->size; i++) {
        new_digit = ((x->digit[i] << n) & DIGIT_MASK) + carry;
        carry = x->digit[i] >> (CARRY_SHIFT - n);
        x->digit[i] = new_digit;
    }
    if (carry) {
        bi_extend(x, x->size + 1);
        x->digit[x->size - 1] = carry;
    }
    return 0;
}

/* bigint shift left n bits for 1 <= n <= CARRY_SHIFT */
int
bi_shr(bigint_t* x, uint n) {
    if (x->size == 0 || n == 0 || n > CARRY_SHIFT) {
        return 0;
    }
    uint i, new_digit, carry = 0;
    for (i = x->size - 1; i >= 0; i--) {
        new_digit = (carry & DIGIT_MASK) + (x->digit[i] >> n);
        carry = x->digit[i] << (CARRY_SHIFT - n);
        x->digit[i] = new_digit;
    }
    bi_normalize(x);
    return 0;
}

bigint_t
bi_abs_add(bigint_t* a, bigint_t* b) {
    uint a_size = a->size, b_size = b->size;
    /* ensure a's size is larger */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        uint tmp_size = a_size; b_size = a_size; a_size = tmp_size;
    }
    bigint_t c = new_bigint(a_size + 1);
    uint i, carry = 0;
    for (i = 0; i < b_size; ++i) {
        carry += a->digit[i] + b->digit[i];
        c.digit[i] = carry & DIGIT_MASK;
        carry >>= CARRY_SHIFT;
    }
    for (; i < a_size; ++i) {
        carry += a->digit[i];
        c.digit[i] = carry & DIGIT_MASK;
        carry >>= CARRY_SHIFT;
    }
    c.digit[i] = carry;
    bi_normalize(&c);
    return c;
}

bigint_t
bi_abs_sub(bigint_t* a, bigint_t* b) {
    uint a_size = a->size, b_size = b->size;
    uint sign = 0;
    /* ensure a's size is larger */
    if (a_size < b_size) {
        bigint_t* tmp = a; a = b; b = tmp;
        uint tmp_size = a_size; b_size = a_size; a_size = tmp_size;
        uint sign = 1;
    }
    /* ensure that a > b */
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
        a_size = b_size = i + 1; /* remaining leading digits must be zero */
    }
    bigint_t c = new_bigint(a_size);
    uint i, borrow = 0;
    for (i = 0; i < b_size; ++i) {
        borrow = a->digit[i] - b->digit[i] - borrow;
        c.digit[i] = borrow & DIGIT_MASK;
        borrow >>= CARRY_SHIFT;
    }
    for (; i < a_size; ++i) {
        borrow = a->digit[i] - borrow;
        c.digit[i] = borrow & DIGIT_MASK;
        borrow >>= CARRY_SHIFT;
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
// bigint_t bi_div(bigint_t* a, bigint_t* b);

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


/* expect string of decimal, heximal, or binary uinteger */
bigint_t
bigint_from_str(const char* str) {
    bigint_t x;
    uint str_length = strlen(str), base = 10;
    /* dec & hex need at least 4 bit per digit */
    uint safe_size = str_length * 4 / 31 + 1;
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
    uint i, carry = 0;
    if (base == 2) {
        x = new_bigint(safe_size);
        /* set it bit by bit */
        for (i = str_length - 1; i >= 0; i--) {
            if (str[i] == '1') {
                uint digit_index = str_length - 1 - i;
                x.digit[digit_index / 31] |= (1 << (digit_index % 31));
            }
        }
    }
    else {
        x = new_bigint(safe_size);
        int cur_digit_num = 0;
        for (i = 0; i < str_length; i++) {
            /* mul base */
            if (i != 0) {
                if (base == 10) {
                    /* x * 10 = (x << 2 + x) << 1 */
                    bigint_t t1 = copy_bigint(&x);
                    bi_shl(&t1, 2);
                    bigint_t t2 = bi_abs_add(&t1, &x);
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
            uint d = str[i];
            if ('0' <= d && d <= '9') {
                d -= '0';
            }
            else if ('a' <= d && d <= 'f') {
                d -= 'a' - 10;
            }
            else if ('A' <= d && d <= 'F') {
                d -= 'A' - 10;
            }
            carry += x.digit[cur_digit_num] + d;
            x.digit[cur_digit_num] = carry & DIGIT_MASK;
            carry >>= CARRY_SHIFT;
            if (carry) {
                cur_digit_num += 1;
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
bigint_from_tens_power(uint exp) {
    bigint_t x = ONE_BIGINT();
    while (exp != 0) {
        if (exp == 1) {
            /* x *= 10 --> x = ((x << 2) + x) << 1 */
            bigint_t t1 = copy_bigint(&x);
            bi_shl(&t1, 2);
            bigint_t t2 = bi_abs_add(&t1, &x);
            bi_shl(&t2, 1);
            free_bigint(&x);
            free_bigint(&t1);
            x = t2;
            exp -= 1;
        }
        else if (exp == 2) {
            /* x *= 100 --> x = ((x << 4) + (x << 3) + x) << 2 */
            bigint_t t1 = copy_bigint(&x);
            bigint_t t2 = copy_bigint(&x);
            bi_shl(&t1, 4);
            bi_shl(&t2, 3);
            bigint_t t3 = bi_abs_add(&t1, &t2);
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
            bigint_t t1 = copy_bigint(&x);
            bigint_t t2 = copy_bigint(&x);
            bi_shl(&t1, 10);
            bi_shl(&t2, 4);
            bigint_t t3 = bi_abs_sub(&t1, &t2);
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
