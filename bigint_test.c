#include <stdio.h>
#include "bigint.h"

int assert(int b) {
    if (b == 0) {
        printf("assertion error\n");
        exit(1);
    }
}

int main() {
    bigint_t zero = bigint_from_str("0");
    bigint_t one = bigint_from_str("1");
    bigint_t two = bigint_from_str("2");
    bigint_t eight = bigint_from_str("8");

    bigint_t mone = bigint_from_str("-1");
    
    bigint_t a   = bigint_from_str("123456789");
    bigint_t b   = bigint_from_str("987654321");
    bigint_t apb = bigint_from_str("1111111110");

    bigint_t c   = bigint_from_str("999999999999999999999999999999");
    bigint_t d   = bigint_from_str("123456789012345678901234567890");
    bigint_t cpd = bigint_from_str("1123456789012345678901234567889");
    bigint_t cmd = bigint_from_str("876543210987654321098765432109");

    bigint_t e = bigint_from_str("9999999999999999999999999999");
    bigint_t f = bigint_from_str("10000000000000000000000000000");

    bigint_t g = bigint_from_str("115792089237316195423570985008687907853269984665640564039457584007913129639932");
    bigint_t h = bigint_from_str("18446744073709551614");
    bigint_t gdh = bigint_from_str("6277101735386680764516354157049543343102891635622409142280");
    
    // bigint_t z = bigint_from_str("0xffffffff");
    // print_bigint_dec(&z); puts("");

    bigint_t result;

    // /* add */
    // result = bi_add(&a, &b);
    // print_bigint_dec(&result); puts("");
    // assert(bi_eq(&result, &apb));

    // result = bi_add(&c, &d);
    // print_bigint_dec(&result); puts("");
    // assert(bi_eq(&result, &cpd));

    // /* sub */
    // result = bi_sub(&c, &d);
    // print_bigint_dec(&result); puts("");
    // assert(bi_eq(&result, &cmd));

    // result = bi_sub(&e, &f);
    // print_bigint_dec(&result); puts("");
    // assert(bi_eq(&result, &mone));

    /* div */
    result = bi_div(&c, &d);
    print_bigint_dec(&result); puts("");
    assert(bi_eq(&result, &eight));

    result = bi_div(&g, &h);
    print_bigint_dec(&result); puts("");
    assert(bi_eq(&result, &gdh));
    return 0;
}