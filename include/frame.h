#include "dynarr.h"
#include "objects.h"

#ifndef FRAME_H
#define FRAME_H

typedef struct frame {
    struct frame* parent;
    dynarr_t objects; /* type: object */
    dynarr_t names; /* type: int */
    int size; /* number of name-object pairs */
} frame_t;

extern frame_t* TOP_FRAME();

extern frame_t* new_frame(
    const frame_t* parent,
    const object_t* init_obj,
    const int init_name
);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t* frame_set(frame_t* f, const int name, const object_t* obj);

extern void pop_frame(frame_t* f);

#endif
