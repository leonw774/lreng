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
    TYPE_CALL,
    TYPE_ANY
} object_type_enum;

typedef unsigned char object_type_t;

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

typedef union object_data {
    number_t number;
    pair_t pair;
    callable_t callable;
} object_data_t;

typedef struct object {
    unsigned char is_error;
    unsigned char is_const;
    object_type_t type;
    unsigned int ref_count;
    object_data_t data;
} object_t;

#define object_data_size sizeof(object_data_t)
#define object_struct_size sizeof(object_t)

extern const object_t* NULL_OBJECT_PTR;
extern const object_t* ERR_OBJECT_PTR;
extern const object_t ERR_OBJECT;

extern const object_t RESERVED_OBJS[RESERVED_ID_NUM];
extern const char* OBJECT_TYPE_STR[OBJECT_TYPE_NUM + 1];

extern object_t* create_object(object_type_enum type, object_data_t data);
extern object_t* copy_object(object_t* obj);
extern void free_object(object_t* obj);
extern int print_object(object_t* obj, char end);

extern int object_eq(object_t* a, object_t* b);
extern int to_bool(object_t* obj);

#endif
