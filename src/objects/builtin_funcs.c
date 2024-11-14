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
        return ERR_OBJERR();
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
        print_runtime_error(0, 0,
            "built-in function 'output': argument type should be number");
    }
    number_t n = obj->data.number;
    unsigned char c;
    if (n.zero) {
        putchar(0);
    }
    else if ((n.sign == 0 && n.denom.size == 1 && n.denom.digit[0] == 1
        && n.numer.size == 1 && n.numer.digit[0] < 256)) {
        putchar(n.numer.digit[0]);
    }
    else {
        print_runtime_error(0, 0,
            "built-in function 'output': argument is not integer in [0, 255]");
        print_number_frac(&obj->data.number); puts("");
        return ERR_OBJERR();
    }
    return OBJ_OBJERR(NULL_OBJECT);
}