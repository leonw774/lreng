#include "dynarr.h"
#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct name_objptr_pair {
    int name;
    object_t* objptr;
} name_objptr_t;

typedef struct frame {
    dynarr_t global_pairs; /* type: name_obj_pair_t */
    /* indexs & stack stores the name obj pairs for call stack */
    dynarr_t indexs; /* type: int */
    dynarr_t stack; /* type: dynarr_t of name_obj_pair_t */
    unsigned int ref_count;
} frame_t;

extern frame_t* frame_new();

extern frame_t* frame_copy(const frame_t* f);

extern void frame_free(frame_t* f);

extern void stack_new(frame_t* f);

extern void stack_push(frame_t* f, const int entry_index);

extern void stack_pop(frame_t* f);

extern void stack_clear(frame_t* f, const int can_free_pairs);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t** frame_set(frame_t* f, const int name, object_t* obj);

#endif
