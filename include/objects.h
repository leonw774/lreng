#include "dynarr.h"
#include "bigint.h"
#include "number.h"
#include "token.h"

#ifndef OBJECT_H
#define OBJECT_H

typedef enum object_type {
    OBJ_NULL,
    OBJ_NUMBER,
    OBJ_PAIR,
    OBJ_FUNC,
    OBJ_BUILTIN_FUNC
} object_type_enum;

typedef struct object object_t;

typedef struct nullobj {
    const short null;
} nullobj_t;

typedef struct pair {
    object_t* left;
    object_t* right;
} pair_t;

typedef struct func {
    int arg_name;
    int root_index;
} func_t;

typedef struct builtin_func {
    int name;
} builtin_func_t;

union object_union {
    nullobj_t null;
    number_t number;
    pair_t pair;
    func_t func;
    builtin_func_t builtin_func;
};

typedef struct object {
    const object_type_enum type;
    union object_union data;
} object_t;


extern object_t new_object(token_t* main, token_t* left, token_t* right);
extern void free_object(object_t* obj);

extern const object_t const RESERVED_OBJS[RESERVED_ID_NUM];

#endif
