#include <stdio.h>
#include "errormsg.h"
#include "objects.h"
#include "builtin_funcs.h"

object_or_error_t
builtin_func_input(object_t* obj) {
    int c;
    if (obj->type != TYPE_NULL) {
        print_runtime_error(
            0, 0, "built-in function 'input': argument type should be null"
        );
    }
    c = getchar();
    return OBJ_OBJERR(
        ((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(c)}
        })
    );
}

object_or_error_t
builtin_func_output(object_t* obj) {
    /* check if obj is number */
    if (obj->type != TYPE_NUMBER) {
        print_runtime_error(
            0, 0, "built-in function 'output': argument type should be number"
        );
    }
    number_t n = obj->data.number;
    bigint_t one = ONE_BIGINT(), byte_max = ONE_BIGINT();
    byte_max.digit[0] = 256;
    unsigned char c;
    if (n.sign == 0 && bi_eq(&n.denom, &one) && bi_lt(&n.numer, &byte_max)) {
        putchar(n.numer.digit[0]);
    }
    else {
        print_runtime_error(
            0, 0, "built-in function 'output': argument is not byte"
        );
    }
    free_bi(&one);
    free_bi(&byte_max);
    return OBJ_OBJERR(NULL_OBJECT);
}