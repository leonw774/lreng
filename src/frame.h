#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct frame_entry {
    int code;
    object_t* object;
} frame_entry_t;

#define TYPE frame_entry_t
#define TYPE_NAME frame_entry
#include "utils/dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

typedef struct frame {
    unsigned int ref_count;

    /* do we own the globals? if true, we can free it */
    unsigned int is_own_globals;

    /* the reference to the global stack that stores shared code-object pairs */
    dynarr_frame_entry_t* globals;

    /* stores the functions that own each stack section */
    dynarr_int_t entry_indexs;

    /* store the start index of each stack section */
    dynarr_int_t stack_pointers;

    /* stores the code-object pairs for function stack on a dynamic array */
    dynarr_frame_entry_t stack;
} frame_t;

extern frame_t* frame_new(const frame_t* parent);

extern frame_t* frame_copy(const frame_t* f);

extern void frame_free(frame_t* f);

extern void frame_call(frame_t* f, const int entry_index);

extern void frame_return(frame_t* f);

extern object_t* frame_get(const frame_t* f, const int code);

extern object_t** frame_set(frame_t* f, const int code, object_t* obj);

extern int frame_print(frame_t* f);

extern frame_t*
frame_call_with_closure(const frame_t* caller_frame, const object_t* func);

#endif
