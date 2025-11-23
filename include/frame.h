#include "dynarr.h"
#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct name_objptr {
    int name;
    object_t* objptr;
} name_objptr_t;

typedef struct frame {
    dynarr_t global_pairs; /* type: name_obj_pair_t */
    /* entry index stores the functions that own each stack section
       type: int */
    dynarr_t entry_indexs;
    /* stack pointers store the start index of each stack section
       type: int */
    dynarr_t stack_pointers;
    /* stack stores the name obj pairs for function stack on a dynamic array
       type: name_obj_pair_t */
    dynarr_t stack;
    unsigned int ref_count;
} frame_t;

extern frame_t* frame_new();

extern frame_t* frame_copy(const frame_t* f);

extern frame_t* frame_copy_globals(const frame_t* f);

extern void frame_free(frame_t* f);

extern void frame_free_stack(frame_t* f);

extern void frame_call(frame_t* f, const int entry_index);

extern void frame_return(frame_t* f);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t** frame_set(frame_t* f, const int name, object_t* obj);

extern int frame_print(frame_t* f);

extern frame_t*
frame_call_with_closure(const frame_t* caller_frame, const object_t* func);

#endif
