#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    object_t* (*func_ptr)(frame_t*, object_t*);
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
    unsigned char err;
    union object_union as;
} object_t;

#define object_data_size sizeof(object_t)

const object_t NULL_OBJECT = (object_t) {
    .type = TYPE_NULL, .err = 0, .as = { .null = 0, },
};
const object_t ERR_OBJECT = (object_t) {
    .type = TYPE_NULL, .err = 1, .as = { .null = 0, },
};
const object_t* NULL_OBJPTR = &NULL_OBJECT;
const object_t* ERR_OBJPTR = &NULL_OBJECT;

int
object_print(object_t* o)
{
    switch (o->type) {
    case TYPE_NULL:
        printf("[Null]");
        break;
    case TYPE_NUM:
        printf("[Num %f]", o->as.number);
        break;
    case TYPE_CALL:
        printf(
            "[Call func=%p, arg=%d, frame=]", o->as.callable.func_ptr,
            o->as.callable.arg_code, o->as.callable.init_frame
        );
        break;
    case TYPE_PAIR:
        printf("[PAIR (");
        object_print(o->as.pair.left);
        printf(", ");
        object_print(o->as.pair.right);
        printf("]");
        break;
    default:
        fprintf(stderr, "built-in function 'debug' bad type: %d\n", o->type);
        return 1;
    }
    return 0;
}

object_t*
from_number(double n)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_NUM, .err = 0, .as = { .number = n, }, };
    return o;
}

object_t*
from_pair(pair_t p)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_PAIR, .err = 0, .as = { .pair = p, }, };
    return o;
}

object_t*
from_callable(callable_t c)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_PAIR, .err = 0, .as = { .callable = c, }, };
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

void
frame_entry_list_free(frame_entry_t* fe)
{
    if (fe) {
        frame_entry_list_free(fe->next);
    }
}

typedef struct frame {
    object_t* (*func_ptr)(frame_t*, object_t*);
    frame_entry_t* frame_entry_list;
    frame_t* next;
    frame_t* prev;
} frame_t;

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
    if (f == NULL) {
        return NULL;
    }
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
frame_push(frame_t* f, object_t* (*func_ptr)(frame_t*, object_t*))
{
    frame_t* newf = malloc(sizeof(frame_t*));
    *newf = (frame_t) {
        .func_ptr = func_ptr,
        .frame_entry_list = NULL,
        .next = f,
        .prev = NULL,
    };
    if (f) {
        f->prev = newf;
    }
    return newf;
}

frame_t*
frame_pop(frame_t* f)
{
    if (f == NULL) {
        return NULL;
    }
    frame_t* new_top = f->next;
    frame_entry_list_free(f->frame_entry_list);
    free(f);
    if (new_top) {
        new_top->prev = NULL;
    }
    return new_top;
}

frame_t*
frame_get_bottom(frame_t* f)
{
    if (f == NULL) {
        return NULL;
    }
    frame_t* tmpf = f;
    while (tmpf->next) {
        tmpf = tmpf->next;
    }
    return tmpf;
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
    new_f->next = frame_copy(f->next);
    new_f->next->prev = new_f;
    new_f->prev = NULL;
    return new_f;
}

frame_t*
frame_get_callee_frame(frame_t* caller_frame, object_t* func_obj)
{
    frame_t* callee_frame;
    frame_t* init_frame = func_obj->as.callable.init_frame;
    frame_t* caller_i;
    frame_t* init_i;
    int is_forked = 0;
    /* if is direct recursion, copy caller frame except last stack section */
    if (caller_frame->func_ptr == func_obj->as.callable.func_ptr) {
        callee_frame = frame_copy(caller_frame);
        callee_frame = frame_pop(callee_frame);
        callee_frame = frame_push(callee_frame, caller_frame->func_ptr);
        return callee_frame;
    }

    /* otherwise, starting from bottom as 0-th entry, if the i-th func_ptr of
     * the function's init-time frame and the i-th func_ptr of the caller
     * frame is the same, then the i-th section of callee frame equals caller
     * frame's i-th section. but once the func_ptr is different, they are in
     * different closure path, the rest of init-time frame stack is used
     * */
    caller_i = frame_get_bottom(caller_frame);
    init_i = frame_get_bottom(init_frame);
    while (init_i->prev != NULL) {
        callee_frame = frame_push(callee_frame, init_i->func_ptr);
        if (!is_forked && caller_i->func_ptr == init_i->func_ptr) {
            callee_frame->frame_entry_list
                = frame_entry_list_copy(caller_i->frame_entry_list);
        } else {
            callee_frame->frame_entry_list
                = frame_entry_list_copy(init_i->frame_entry_list);
            is_forked = 1;
        }
        init_i = init_i->prev;
        caller_i = caller_i ? caller_i->prev : caller_i;
    }
    /* then push the new stack section and entry index to callee frame */
    callee_frame = frame_push(callee_frame, caller_frame->func_ptr);
    return callee_frame;
}

object_t*
exec_call(frame_t* caller_frame, callable_t* callable, object_t* arg)
{
    if (callable->init_frame == NULL) {
        object_t* result = callable->func_ptr(caller_frame, arg);
    }
}
object_t* top(frame_t* FRAME, object_t* arg);

object_t*
input(frame_t* FRAME, object_t* arg)
{
    int c = getchar();
    return c == EOF ? (object_t*)NULL_OBJPTR : from_number(c);
}

object_t*
output(frame_t* FRAME, object_t* arg)
{
    int c = as_number(arg);
    const char* err_msg_not_byte_number
        = "built-in function 'output': argument is not in [0, 255], but %d\n";
    const char* err_msg_failed_to_write
        = "built-in function 'output': failed to write to stdout.\n";
    if (!(0 <= c && c <= 255)) {
        fprintf(stderr, err_msg_not_byte_number, c);
        return (object_t*)ERR_OBJPTR;
    }
    if (putc(c, stdout) == EOF) {
        fprintf(stderr, err_msg_failed_to_write);
        return (object_t*)ERR_OBJPTR;
    }
    return (object_t*)NULL_OBJPTR;
}

object_t*
error(frame_t* FRAME, object_t* arg)
{
    int c = as_number(arg);
    const char* err_msg_not_byte_number
        = "built-in function 'output': argument is not in [0, 255], but %d\n";
    const char* err_msg_failed_to_write
        = "built-in function 'output': failed to write to stdout.\n";
    if (!(0 <= c && c <= 255)) {
        fprintf(stderr, err_msg_not_byte_number, c);
        return (object_t*)ERR_OBJPTR;
    }
    if (putc(c, stderr) == EOF) {
        fprintf(stderr, err_msg_failed_to_write);
        return (object_t*)ERR_OBJPTR;
    }
    return (object_t*)NULL_OBJPTR;
}

object_t*
is_number(frame_t* FRAME, object_t* arg)
{
    return from_number(arg->type == TYPE_NUM);
}

object_t*
is_callable(frame_t* FRAME, object_t* arg)
{
    return from_number(arg->type == TYPE_CALL);
}

object_t*
is_pair(frame_t* FRAME, object_t* arg)
{
    return from_number(arg->type == TYPE_PAIR);
}

object_t*
debug(frame_t* FRAME, object_t* arg)
{
    int error = object_print(arg);
    return (object_t*)(error ? ERR_OBJPTR : NULL_OBJPTR);
}

void
prepare_reserve(frame_t* top_frame)
{
    object_t* var_0 = (object_t*)NULL_OBJPTR;
    object_t* var_1 = from_callable((callable_t) {
        .func_ptr = input, .arg_code = -1, .init_frame = NULL });
    object_t* var_2 = from_callable((callable_t) {
        .func_ptr = output, .arg_code = -1, .init_frame = NULL });
    object_t* var_3 = from_callable((callable_t) {
        .func_ptr = is_number, .arg_code = -1, .init_frame = NULL });
    object_t* var_4 = from_callable((callable_t) {
        .func_ptr = is_callable, .arg_code = -1, .init_frame = NULL });
    object_t* var_5 = from_callable((callable_t) {
        .func_ptr = is_pair, .arg_code = -1, .init_frame = NULL });
    object_t* var_6 = from_callable((callable_t) {
        .func_ptr = debug, .arg_code = -1, .init_frame = NULL });
    frame_set(top_frame, 0, var_0);
    frame_set(top_frame, 1, var_1);
    frame_set(top_frame, 2, var_2);
    frame_set(top_frame, 3, var_3);
    frame_set(top_frame, 4, var_4);
    frame_set(top_frame, 5, var_5);
    frame_set(top_frame, 6, var_6);
}

int
main()
{
    frame_t* top_frame = malloc(sizeof(frame_t));
    *top_frame = (frame_t) {
        .func_ptr = top,
        .frame_entry_list = NULL,
        .next = NULL,
        .prev = NULL,
    };
    prepare_reserve(top_frame);
    top(top_frame, NULL);
    return 0;
}
