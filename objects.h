#include "dynarr.h"
#include "bigint.h"
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

typedef union object object_t;

typedef struct nullobj {
    const object_type_enum type;
} nullobj_t;

typedef struct number {
    const object_type_enum type;
    bigint_t numer;
    bigint_t denom;
} number_t;

typedef struct pair {
    const object_type_enum type;
    object_t* left;
    object_t* right;
} pair_t;

typedef struct func {
    const object_type_enum type;
    int arg_name;
    int root_index;
} func_t;

typedef struct builtin_func {
    const object_type_enum type;
    int name;
} builtin_func_t;

union object {
    nullobj_t null;
    number_t number;
    pair_t pair;
    func_t func;
    builtin_func_t builtin_func;
};

extern object_t new_object(token_t* main, token_t* left, token_t* right);

extern number_t number_from_str(char* str);

void free_object(object_t* obj);

extern const object_t const RESERVED_OBJS[RESERVED_ID_NUM];

#endif
