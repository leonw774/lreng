#include <stdio.h>
#include <stdlib.h>
#include "number.h"

int
assert(int b) {
    if (b == 0) {
        printf("assertion error\n");
        exit(1);
    }
}

int
main() {
    number_t onehundred = number_from_str("100");
    number_t onequarter = number_from_str("0.25");
    print_number_frac(&onehundred); puts("");
    print_number_frac(&onequarter); puts("");

    number_t a = number_from_str("1.1");
    number_t b = number_from_str("2.2");
    number_t apb = number_from_str("3.3");

    number_t c = number_from_str("135135135135135135.246246246246246246");
    number_t d = number_from_str("777777777777777.888888888888888");
    number_t cpd = number_from_str("135912912912912913.135135135135134246");
    number_t csd = number_from_str("134357357357357357.357357357357358246");

    number_t result;

    result = number_add(&a, &b);
    print_number_frac(&a); puts(" + "); print_number_frac(&b); puts(" =");
    print_number_frac(&result); puts("");
    assert(number_eq(&result, &apb));

    result = number_add(&c, &d);
    print_number_frac(&c); puts(" + "); print_number_frac(&d); puts(" =");
    print_number_frac(&result); puts("");
    assert(number_eq(&result, &cpd));

    result = number_sub(&c, &d);
    print_number_frac(&c); puts(" - "); print_number_frac(&d); puts(" =");
    print_number_frac(&result); puts("");
    assert(number_eq(&result, &csd));

    number_t pi = number_from_str("3.14159265");
    print_number_dec(&pi, 3); puts("");
    return 0;
}