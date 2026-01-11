#include "dynarr.h"
#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct name_objptr {
    int name;
    object_t* objptr;
} name_objptr_t;

typedef struct frame {
    unsigned int ref_count;

    /* do we own the globals? if true, we can free it */
    unsigned int is_own_globals;

    /* the reference to the global stack that stores global name-object pairs
       type: name_objptr_t */
    dynarr_t* globals;

    /* entry index stores the functions that own each stack section
       type: int */
    dynarr_t entry_indexs;

    /* stack pointers store the start index of each stack section
       type: int */
    dynarr_t stack_pointers;

    /* stack stores the name-object pairs for function stack on a dynamic array
       type: name_objptr_t */
    dynarr_t stack;
} frame_t;

extern frame_t* frame_new(const frame_t* parent);

extern frame_t* frame_copy(const frame_t* f);

extern void frame_free(frame_t* f);

extern void frame_call(frame_t* f, const int entry_index);

extern void frame_return(frame_t* f);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t** frame_set(frame_t* f, const int name, object_t* obj);

extern int frame_print(frame_t* f);

extern frame_t*
frame_call_with_closure(const frame_t* caller_frame, const object_t* func);

#endif
