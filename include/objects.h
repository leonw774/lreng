#include "dynarr.h"
#include "bigint.h"
#include "number.h"
#include "token.h"
#include "tree.h"

#ifndef OBJECT_H
#define OBJECT_H

typedef enum object_type {
    TYPE_NULL,
    TYPE_NUM,
    TYPE_PAIR,
    TYPE_FUNC,
    TYPE_BUILTIN_FUNC,
    TYPE_ANY
} object_type_enum;

#define OBJECT_TYPE_NUM TYPE_ANY

typedef struct object object_t;

typedef struct pair {
    object_t* left;
    object_t* right;
} pair_t;

typedef struct frame frame_t;

typedef struct callable {
    int is_macro;
    int builtin_name; /* -1 if is not builtin function */
    int index; /* the index on tree */
    int arg_name; /* -1 if no arg */
    frame_t* init_time_frame;
} callable_t;

#define NOT_BUILTIN_FUNC -1

union object_union {
    number_t number;
    pair_t pair;
    callable_t callable;
};

typedef struct object {
    object_type_enum type;
    union object_union data;
} object_t;

typedef struct object_or_error {
    unsigned char is_error;
    object_t obj;
} object_or_error_t;

#define object_union_size sizeof(union object_union)
#define object_struct_size sizeof(object_t)
#define object_or_error_struct_size sizeof(object_or_error_t)

#define NULL_OBJECT ((object_t) {\
    .type = TYPE_NULL, .data = {.number = EMPTY_NUMBER()}})
#define ERR_OBJERR() ((object_or_error_t) {.is_error = 1, .obj = NULL_OBJECT})
#define OBJ_OBJERR(o) ((object_or_error_t) {.is_error = 0, .obj = o})

extern const object_t const RESERVED_OBJS[RESERVED_ID_NUM];
extern const char* OBJECT_TYPE_STR[OBJECT_TYPE_NUM + 1];

/* deep copy an object */
extern object_t copy_object(const object_t* obj);
extern void free_object(object_t* obj);
extern int print_object(object_t* obj, char end);

extern int object_eq(object_t* a, object_t* b);
extern int to_bool(object_t* obj);

#endif
