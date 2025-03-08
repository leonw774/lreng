#include <stdio.h>
#include <stdlib.h>
#include "number.h"

void
assert(int b) {
    if (b == 0) {
        printf("assertion error\n");
        exit(1);
    }
}

int
main() {
    number_t mone_hundred = number_from_str("-100");
    number_t one_quarter = number_from_str("0.25");
    print_number_frac(&mone_hundred, '\n');
    print_number_frac(&one_quarter, '\n');
    number_t two_power_two_hundred = number_from_str("1606938044258990275541962092341162602522202993782792835301376");
    print_number_frac(&two_power_two_hundred, '\n');

    number_t a = number_from_str("1.1");
    number_t b = number_from_str("2.2");
    number_t apb = number_from_str("3.3");

    number_t c = number_from_str("135135135135135135.246246246246246246");
    number_t d = number_from_str("777777777777777.888888888888888");
    number_t cpd = number_from_str("135912912912912913.135135135135134246");
    number_t csd = number_from_str("134357357357357357.357357357357358246");

    number_t result;

    result = number_add(&a, &b);
    print_number_frac(&a, '\0'); puts(" + ");
    print_number_frac(&b, '\0'); puts(" =");
    print_number_frac(&result, '\n');
    assert(number_eq(&result, &apb));
    free_number(&result);

    result = number_add(&c, &d);
    print_number_frac(&c, '\0'); puts(" + ");
    print_number_frac(&d, '\0'); puts(" =");
    print_number_frac(&result, '\n');
    assert(number_eq(&result, &cpd));
    free_number(&result);

    result = number_sub(&c, &d);
    print_number_frac(&c, '\0'); puts(" - ");
    print_number_frac(&d, '\0'); puts(" =");
    print_number_frac(&result, '\n');
    assert(number_eq(&result, &csd));
    free_number(&result);

    result = number_exp(&one_quarter, &mone_hundred);
    print_number_frac(&one_quarter, '\0'); puts(" ^ ");
    print_number_frac(&mone_hundred, '\0'); puts(" =");
    print_number_frac(&result, '\n');
    assert(number_eq(&result, &two_power_two_hundred));

    number_t pi = number_from_str("3.1415926535");
    print_number_dec(&pi, 4, '\n');
    print_number_dec(&pi, 7, '\n');
    
    printf("all passed");
    return 0;
}