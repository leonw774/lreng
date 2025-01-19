#include <stdio.h>
#include <stdlib.h>
#include "errormsg.h"
#include "bigint.h"
#include "objects.h"
#include "builtin_funcs.h"

/* correspond to reserved_id_name */
object_t* (*BUILDTIN_FUNC_ARRAY[RESERVED_ID_NUM])(object_t*) = {
    NULL,
    &builtin_func_input,
    &builtin_func_output
}; 

object_t*
builtin_func_input(object_t* obj) {
    int c;
    if (obj->type != TYPE_NULL) {
        sprintf(
            ERR_MSG_BUF,
            "built-in function 'input': argument type should be null"
        );
        return (object_t*) ERR_OBJECT_PTR;
    }
    c = getchar();
    return create_object(
        TYPE_NUM,
        (object_data_t) number_from_i32(c)
    );
}

object_t*
builtin_func_output(object_t* obj) {
    /* check if obj is number */
    if (obj->type != TYPE_NUM) {
        sprintf(
            ERR_MSG_BUF,
            "built-in function 'output': argument is not a number"
        );
        print_object(obj, '\n');
        return (object_t*) ERR_OBJECT_PTR;
    }
    number_t n = obj->data.number;
    unsigned char c;
    if (n.numer.size == 0) {
        putchar(0);
    }
    else if ((n.numer.sign == 0 && n.denom.size == 1 && n.denom.digit[0] == 1
        && n.numer.size == 1 && n.numer.digit[0] < 256)) {
        putchar(n.numer.digit[0]);
    }
    else {
        dynarr_t numer_dynarr = bi_to_dec_str(&n.numer),
            denom_dynarr = bi_to_dec_str(&n.denom);
        char *numer_str = to_str(&numer_dynarr),
            *denom_str = to_str(&denom_dynarr);
        sprintf(
            ERR_MSG_BUF,
            "built-in function 'output': argument is not integer in [0, 255]"
            ", but [Number] %s(%s, %s)",
            n.numer.sign ? "-" : "",
            numer_str,
            denom_str
        );
        free(numer_str);
        free(denom_str);
        free_dynarr(&numer_dynarr);
        free_dynarr(&denom_dynarr);
        return (object_t*) ERR_OBJECT_PTR;
    }
    return (object_t*) NULL_OBJECT_PTR;
}
