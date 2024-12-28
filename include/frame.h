#include "dynarr.h"
#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct name_object_pair {
    int name;
    object_t object;
} name_obj_pair_t;

typedef struct frame {
    dynarr_t entry_indices; /* type: int */
    dynarr_t stack; /* type: dynarr_t of name_obj_pair_t */
    int refer_count;
} frame_t;

extern frame_t* new_frame(const int entry_index);

extern frame_t* empty_frame();

extern void push_stack(frame_t* f, const int entry_index);

extern void pop_stack(frame_t* f);

extern frame_t* copy_frame(const frame_t* f);

extern void free_frame(frame_t* f, const int can_free_pairs);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t* frame_set(frame_t* f, const int name, const object_t* obj);


#endif
