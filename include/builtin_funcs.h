#include "objects.h"

extern const object_or_error_t (*BUILDTIN_FUNC_ARRAY[RESERVED_ID_NUM])(object_t*); 
extern object_or_error_t builtin_func_input(object_t* number_obj);
extern object_or_error_t builtin_func_output(object_t* number_obj);
