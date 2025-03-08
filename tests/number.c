#include <stdio.h>
#include <stdlib.h>
#include "assert.h"
#include "number.h"

int
main() {
    number_t mone_hundred = number_from_str("-100");
    number_t one_quarter = number_from_str("0.25");
    number_print_frac(&mone_hundred, '\n');
    number_print_frac(&one_quarter, '\n');
    number_t two_power_two_hundred = number_from_str("1606938044258990275541962092341162602522202993782792835301376");
    number_print_frac(&two_power_two_hundred, '\n');

    number_t a = number_from_str("1.1");
    number_t b = number_from_str("2.2");
    number_t apb = number_from_str("3.3");

    number_t c = number_from_str("135135135135135135.246246246246246246");
    number_t d = number_from_str("777777777777777.888888888888888");
    number_t cpd = number_from_str("135912912912912913.135135135135134246");
    number_t csd = number_from_str("134357357357357357.357357357357358246");

    number_t result;

    result = number_add(&a, &b);
    number_print_frac(&a, '\0'); puts(" + ");
    number_print_frac(&b, '\0'); puts(" =");
    number_print_frac(&result, '\n');
    assert(number_eq(&result, &apb));
    number_free(&result);

    result = number_add(&c, &d);
    number_print_frac(&c, '\0'); puts(" + ");
    number_print_frac(&d, '\0'); puts(" =");
    number_print_frac(&result, '\n');
    assert(number_eq(&result, &cpd));
    number_free(&result);

    result = number_sub(&c, &d);
    number_print_frac(&c, '\0'); puts(" - ");
    number_print_frac(&d, '\0'); puts(" =");
    number_print_frac(&result, '\n');
    assert(number_eq(&result, &csd));
    number_free(&result);

    result = number_exp(&one_quarter, &mone_hundred);
    number_print_frac(&one_quarter, '\0'); puts(" ^ ");
    number_print_frac(&mone_hundred, '\0'); puts(" =");
    number_print_frac(&result, '\n');
    assert(number_eq(&result, &two_power_two_hundred));

    number_t pi = number_from_str("3.1415926535");
    number_print_dec(&pi, 4, '\n');
    number_print_dec(&pi, 7, '\n');
    
    printf("all passed");
    return 0;
}