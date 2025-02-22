#include "reserved.h"
#include "objects.h"

extern object_t* (*BUILDTIN_FUNC_ARRAY[RESERVED_ID_NUM])(object_t*); 
extern object_t* builtin_func_input(object_t* number_obj);
extern object_t* builtin_func_output(object_t* number_obj);
extern object_t* builtin_func_is_number(object_t* obj);
extern object_t* builtin_func_is_callable(object_t* obj);
extern object_t* builtin_func_is_pair(object_t* obj);
