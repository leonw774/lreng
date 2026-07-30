#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

double
floor_modulo(double x, double y)
{
    return x - y * floor(x / y);
}

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
    union object_union as;
} object_t;

#define object_data_size sizeof(object_t)

const object_t NULL_OBJECT = (object_t) {
    .type = TYPE_NULL, .as = { .null = 0, },
};
const object_t* NULL_OBJPTR = &NULL_OBJECT;

void
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
            "[Call func=%p, arg=%d, frame=%p]", o->as.callable.func_ptr,
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
        exit(EXIT_FAILURE);
    }
}

double
object_equal(object_t* x, object_t* y)
{
    if (x->type == y->type) {
        switch (x->type) {
        case TYPE_NULL:
            return 1.f;
        case TYPE_NUM:
            return x->as.number == y->as.number ? 1.f : 0.f;
        case TYPE_PAIR:
            return object_equal(x->as.pair.left, y->as.pair.left) == 1.f
                && object_equal(x->as.pair.right, y->as.pair.right) == 1.f;
        case TYPE_CALL:
            return x->as.callable.func_ptr == y->as.callable.func_ptr
                && x->as.callable.arg_code == y->as.callable.arg_code
                && x->as.callable.init_frame == y->as.callable.init_frame;
        default:
            fprintf(stderr, "object_equal bad type: %d\n", x->type);
            exit(EXIT_FAILURE);
        }
    }
    return 0.f;
}

object_t*
from_number(double n)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_NUM, .as = { .number = n, }, };
    return o;
}

object_t*
from_pair(pair_t p)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_PAIR, .as = { .pair = p, }, };
    return o;
}

object_t*
from_callable(callable_t c)
{
    object_t* o = malloc(sizeof(object_t));
    *o = (object_t) { .type = TYPE_CALL, .as = { .callable = c, }, };
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
    frame_t* newf = malloc(sizeof(frame_t));
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
    if (f->next) {
        new_f->next = frame_copy(f->next);
        new_f->next->prev = new_f;
    }
    new_f->prev = NULL;
    return new_f;
}

frame_t*
frame_get_callee_frame(frame_t* caller_frame, callable_t* callable)
{
    frame_t* callee_frame = NULL;
    frame_t* init_frame = callable->init_frame;
    frame_t* caller_i;
    frame_t* init_i;
    int is_forked = 0;
    /* if is direct recursion, copy caller frame except last stack section */
    if (caller_frame->func_ptr == callable->func_ptr) {
        callee_frame = frame_copy(caller_frame);
        callee_frame = frame_pop(callee_frame);
        callee_frame = frame_push(callee_frame, caller_frame->func_ptr);
        return callee_frame;
    }

    /* with bottom as 0-th, if the i-th func_ptr of the init-frame and caller
     * frame is the same, the i-th frame of callee frame is caller's i-th frame.
     * otherwise, they are in different closure path, the rest of init-time
     * frame stack is used */
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
        caller_i = caller_i ? caller_i->prev : NULL;
    }
    /* then push the new stack section and entry index to callee frame */
    callee_frame = frame_push(callee_frame, caller_frame->func_ptr);
    return callee_frame;
}

object_t*
exec_call(frame_t* caller_frame, object_t* callable_obj, object_t* arg)
{
    callable_t callable = as_callable(callable_obj);
    if (callable.init_frame == NULL) {
        return callable.func_ptr(caller_frame, arg);
    }
    frame_t* callee_frame = frame_get_callee_frame(caller_frame, &callable);
    frame_set(callee_frame, callable.arg_code, arg);
    return callable.func_ptr(caller_frame, arg);
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
        exit(EXIT_FAILURE);
    }
    if (putc(c, stdout) == EOF) {
        fprintf(stderr, err_msg_failed_to_write);
        exit(EXIT_FAILURE);
    }
    fflush(stdout);
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
        exit(EXIT_FAILURE);
    }
    if (putc(c, stderr) == EOF) {
        fprintf(stderr, err_msg_failed_to_write);
        exit(EXIT_FAILURE);
    }
    fflush(stderr);
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
    object_print(arg);
    printf("\n");
    fflush(stdout);
    return (object_t*)NULL_OBJPTR;
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
    top(top_frame, NULL);
    return 0;
}

/* transpiled code below */
