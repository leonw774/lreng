#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "objects.h"
#include "frame.h"

frame_t*
TOP_FRAME() {
    frame_t* f = calloc(1, sizeof(frame_t));
    f->parent = NULL;
    f->objects = new_dynarr(sizeof(object_t));
    f->names = new_dynarr(sizeof(int));
    f->size = RESERVED_ID_NUM;
    int i;
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        append(&f->objects, &RESERVED_OBJS[i]);
        append(&f->names, &i);
    }
    return f;
}

frame_t*
new_frame(
    const frame_t* parent,
    const object_t* init_obj,
    const int init_name
) {
    frame_t* f = calloc(1, sizeof(frame_t));
    // printf("new_frame: parent=%p, init_obj=%p name=%d\n",
    //     parent, init_obj, init_name);
    f->parent = (frame_t*) parent;
    f->objects = new_dynarr(sizeof(object_t));
    f->names = new_dynarr(sizeof(int));
    if (init_obj != NULL) {
        object_t clone_init_obj = copy_object(init_obj);
        append(&f->objects, &clone_init_obj);
        append(&f->names, &init_name);
        f->size = 1;
    }
    else {
        f->size = 0;
    }
    return f;
}

object_t*
frame_get(const frame_t* f, const int name) {
    // printf("frame_get: frame=%p, name=%d\n", f, name);
    int i;
    for (i = 0; i < f->names.size; i++) {
        if (name == ((int*) f->names.data)[i]) {
            return &(((object_t*) f->objects.data)[i]);
        }
    }
    if (f->parent == NULL) {
        return NULL;
    }
    if (f == f->parent) {
        printf("a frame's parent is it self\n");
        exit(1);
    }
    return frame_get(f->parent, name);
}

/* return where the object was set */
object_t*
frame_set(frame_t* f, const int name, const object_t* obj) {
    object_t* found_obj = frame_get(f, name);
    /* need to deep copy because of pair */
    object_t clone_obj = copy_object(obj);
    if (found_obj == NULL) {
        append(&f->names, &name);
        append(&f->objects, &clone_obj);
        return back(&f->objects);
    }
    else {
        free_object(found_obj);
        *found_obj = clone_obj;
        return found_obj;
    }
}

/* free frame f and set it to its parent */
void pop_frame(frame_t* f) {
    frame_t* parent = f->parent;
    int i;
    for (i = 0; i < f->objects.size; i++) {
        free_object(&((object_t*) f->objects.data)[i]);
    }
    free_dynarr(&f->objects);
    free_dynarr(&f->names);
    if (parent != NULL) {
        free(f);
        *f = *parent;
    }
    else {
        f = NULL;
    }
}