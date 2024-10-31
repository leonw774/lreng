#include <stdio.h>
#include "../objects/number.h"

int assert(int b) {
    if (b == 0) {
        printf("assertion error\n");
        exit(1);
    }
}

int main() {
    number_t onehundred = number_from_str("100");
    number_t two = number_from_str("2");

    number_t a = number_from_str("1.1");
    number_t b = number_from_str("2.2");

    print_number_frac(&onehundred); puts("");
    print_number_frac(&two); puts("");
    print_number_frac(&a); puts("");
    print_number_frac(&b); puts("");
    return 0;
}