#include "objects.h"
#include "reserved.h"

extern object_t* (*BUILDTIN_FUNC_ARRAY[RESERVED_ID_NUM])(const object_t*);
extern object_t* builtin_func_input(const object_t* number_obj);
extern object_t* builtin_func_output(const object_t* number_obj);
extern object_t* builtin_func_is_number(const object_t* obj);
extern object_t* builtin_func_is_callable(const object_t* obj);
extern object_t* builtin_func_is_pair(const object_t* obj);
