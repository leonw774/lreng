#include "builtin_funcs.h"

#include <stdio.h>
#include <stdlib.h>

#include "bigint.h"
#include "errormsg.h"
#include "objects.h"

/* correspond to reserved_id_name in reserved.h */
object_t* (*BUILDTIN_FUNC_ARRAY[RESERVED_ID_NUM])(const object_t*) = {
    NULL, /* RESERVED_ID_NAME_NULL */
    &builtin_func_input, /* RESERVED_ID_NAME_INPUT */
    &builtin_func_output, /* RESERVED_ID_NAME_OUTPUT */
    &builtin_func_is_number, /* RESERVED_ID_NAME_IS_NUMBER */
    &builtin_func_is_callable, /* RESERVED_ID_NAME_IS_CALLABLE */
    &builtin_func_is_pair, /* RESERVED_ID_NAME_IS_PAIR */
};

object_t*
builtin_func_input(const object_t* obj)
{
    int c;
    const char err_msg[]
        = "built-in function 'input': argument type should be null";
    if (obj->type != TYPE_NULL) {
        sprintf(ERR_MSG_BUF, err_msg);
        return (object_t*)ERR_OBJECT_PTR;
    }
    c = getchar();
    if (c != EOF) {
        return object_create(TYPE_NUM, (object_data_t)number_from_i32(c));
    }
    return (object_t*)NULL_OBJECT_PTR;
}

object_t*
builtin_func_output(const object_t* obj)
{
    const char* err_msg_not_number
        = "built-in function 'output': argument is not a number. Get %s.";
    const char* err_msg_not_byte_number
        = "built-in function 'output': argument is not integer in [0, 255], "
          "but [Number] (%s, %s)";
    /* check if obj is number */
    if (obj->type != TYPE_NUM) {
        sprintf(ERR_MSG_BUF, err_msg_not_number, OBJ_TYPE_STR[obj->type]);
        return (object_t*)ERR_OBJECT_PTR;
    }
    if (obj->data.number.numer.size == 0) {
        putchar(0);
    } else {
        int is_int = obj->data.number.denom.size == 1
            && obj->data.number.denom.digit[0] == 1;
        int is_less_than_256 = obj->data.number.numer.size == 1
            && obj->data.number.numer.digit[0] < 256;
        if (obj->data.number.numer.sign == 0 && is_int && is_less_than_256) {
            putchar(obj->data.number.numer.digit[0]);
        } else {
            dynarr_t numer_dynarr = bi_to_dec_str(&obj->data.number.numer),
                     denom_dynarr = bi_to_dec_str(&obj->data.number.denom);
            char *numer_str = to_str(&numer_dynarr),
                 *denom_str = to_str(&denom_dynarr);
            sprintf(
                ERR_MSG_BUF, err_msg_not_byte_number,
                numer_str ? numer_str : "(null)",
                denom_str ? denom_str : "(null)"
            );
            free(numer_str);
            free(denom_str);
            dynarr_free(&numer_dynarr);
            dynarr_free(&denom_dynarr);
            return (object_t*)ERR_OBJECT_PTR;
        }
    }
    return (object_t*)NULL_OBJECT_PTR;
}

object_t*
builtin_func_is_number(const object_t* obj)
{
    return object_create(
        TYPE_NUM, (object_data_t)number_from_i32(obj->type == TYPE_NUM)
    );
}

object_t*
builtin_func_is_callable(const object_t* obj)
{
    return object_create(
        TYPE_NUM, (object_data_t)number_from_i32(obj->type == TYPE_CALL)
    );
}

object_t*
builtin_func_is_pair(const object_t* obj)
{
    return object_create(
        TYPE_NUM, (object_data_t)number_from_i32(obj->type == TYPE_PAIR)
    );
}
