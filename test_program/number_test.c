#include <stdio.h>
#include "../objects/number.h"

int assert(int b) {
    if (b == 0) {
        printf("assertion error\n");
        exit(1);
    }
}

int main() {
    number_t n = number_from_str("1");
    print_number_frac(&n); puts("");
    return 0;
}