#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#define TYPE_NULL 0
#define TYPE_NUM 1
#define TYPE_PAIR 2
#define TYPE_CALL 3

typedef struct object object_t;

typedef struct pair {
    object_t* left;
    object_t* right;
} pair_t;

typedef struct frame frame_t;

typedef struct callable {
    void (*func_ptr)(frame_t*);
    frame_t* init_frame;
    int arg_code;
} callable_t;

union object_union {
    int null;
    double number;
    pair_t pair;
    callable_t callable;
};

typedef struct object {
    unsigned char type;
    unsigned char is_error;
    union object_union as;
} object_t;

#define object_data_size sizeof(object_t)

const object_t NULL_OBJECT = (object_t) {
    .type = TYPE_NULL,
    .as = { .null = 0, },
};

const object_t* NULL_OBJPTR = &NULL_OBJECT;

object_t*
from_number(double n)
{
    object_t* o = malloc(sizeof(object_t)); 
    *o = (object_t) {
        .type = TYPE_NUM,
        .as = { .number = n, },
    };
    return o;
}

object_t*
from_pair(pair_t p)
{
    object_t* o = malloc(sizeof(object_t)); 
    *o = (object_t) {
        .type = TYPE_PAIR,
        .as = { .pair = p, },
    };
    return o;
}

object_t*
from_callable(callable_t c)
{
    object_t* o = malloc(sizeof(object_t)); 
    *o = (object_t) {
        .type = TYPE_PAIR,
        .as = { .callable = c, },
    };
    return o;
}

int
as_null(object_t* o)
{
    assert(o->type == TYPE_NULL);
    return 0;
}

double
as_number(object_t* o)
{
    assert(o->type == TYPE_NUM);
    return o->as.number;
}

pair_t
as_pair(object_t* o)
{
    assert(o->type == TYPE_PAIR);
    return o->as.pair;
}

callable_t
as_callable(object_t* o)
{
    assert(o->type == TYPE_CALL);
    return o->as.callable;
}


typedef struct frame_entry {
    int code;
    object_t* object;
    struct frame_entry* next;
} frame_entry_t;

frame_entry_t*
frame_entry_list_copy(frame_entry_t* fe)
{
    if (fe == NULL) {
        return NULL;
    }
    frame_entry_t* new_fe = malloc(sizeof(frame_entry_t));
    new_fe->code = fe->code;
    new_fe->object = fe->object;
    new_fe->next = frame_entry_list_copy(fe->next);
    return new_fe;
}

int
frame_entry_list_free(frame_entry_t* fe)
{
    if (fe == NULL) {
        return 0;
    }
    frame_entry_list_free(fe->next);
    return 1;
}

typedef struct frame {
    void (*func_ptr)(frame_t*);
    frame_entry_t* frame_entry_list;
    frame_t* next;
} frame_t;

void top(frame_t* FRAME);

object_t*
frame_get(frame_t* f, int code)
{
    frame_t* cur_frame = f;
    while (cur_frame) {
        frame_entry_t* fe_item = cur_frame->frame_entry_list;
        while (fe_item) {
            if (fe_item->code == code) {
                return fe_item->object;
            }
            fe_item = fe_item->next;
        }
        cur_frame = cur_frame->next;
    }
    return NULL;
}

object_t*
frame_set(frame_t* f, int code, object_t* obj)
{
    frame_entry_t* fe_item = malloc(sizeof(frame_entry_t));
    *fe_item = (frame_entry_t) {
        .code = code,
        .object = obj,
        .next = f->frame_entry_list,
    };
    f->frame_entry_list = fe_item;
    return fe_item->object;
}

frame_t*
frame_push(frame_t* f, void (*func_ptr)(frame_t*))
{
    frame_t* newf = malloc(sizeof(frame_t*));
    *newf = (frame_t) {
        .func_ptr = func_ptr,
        .frame_entry_list = NULL,
        .next = f,
    };
    return newf;
}

frame_t*
frame_pop(frame_t* f)
{
    frame_t* new_top = f->next;
    frame_entry_list_free(f->frame_entry_list);
    if (f->func_ptr != top) {
        free(f);
    }
    return new_top;
}

frame_t*
frame_copy(frame_t* f)
{
    if (f == NULL) {
        return NULL;
    }
    frame_t* new_f = malloc(sizeof(frame_t));
    new_f->func_ptr = f->func_ptr;
    new_f->frame_entry_list = frame_entry_list_copy(f->frame_entry_list);
    new_f->next = frame_copy(f);
    return new_f;
}

// frame_t*
// frame_get_callee_frame(frame_t* caller_frame, object_t* func_obj)
// {
//     frame_t* callee_frame;
//     int i, is_forked = 0;
//     if (caller_frame->func_ptr == func_obj->callable.func_ptr) {
//         /* if is direct recursion */
//     }
// }

int
main()
{
    frame_t top_frame = (frame_t) {
        .func_ptr = top,
        .frame_entry_list = NULL,
        .next = NULL,
    };
    top(&top_frame);
    return 0;
}

