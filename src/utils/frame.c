#include <string.h>
#include "token.h"
#include "objects.h"
#include "frame.h"

const frame_t
TOP_FRAME() {
    frame_t f;
    f.parent = NULL;
    f.objects = new_dynarr(sizeof(object_t));
    f.names = new_dynarr(sizeof(int));
    f.size = RESERVED_ID_NUM;
    int i;
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        append(&f.objects, &RESERVED_OBJS[i]);
        append(&f.names, &i);
    }
    return f;
}

frame_t
new_frame(const frame_t* parent, const object_t* init_obj, const int init_name) {
    frame_t f;
    f.parent = (frame_t*) parent;
    f.objects = new_dynarr(sizeof(object_t));
    f.names = new_dynarr(sizeof(int));
    if (init_obj != NULL) {
        append(&f.objects, init_obj);
        append(&f.names, &init_name);
        f.size = 1;
    }
    else {
        f.size = 0;
    }
    return f;
}

object_t*
frame_get(const frame_t* f, const int name) {
    int i;
    for (i = 0; i < f->names.size; i++) {
        if (name == ((int*) f->names.data)[i]) {
            return &(((object_t*) f->objects.data)[i]);
        }
    }
    if (f->parent == NULL) {
        return NULL;
    }
    return frame_get(f->parent, name);
}

object_t*
frame_find(const frame_t* f, const int name) {
    int i;
    for (i = 0; i < f->names.size; i++) {
        if (name == ((int*) f->names.data)[i]) {
            return ((object_t*) f->objects.data) + i;
        }
    }
    if (f->parent == NULL) {
        return NULL;
    }
    return frame_get(f->parent, name);
}

/* return where the object was set */
object_t*
frame_set(frame_t* f, const int name, const object_t* obj) {
    object_t* found_obj = frame_find(f, name);
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
        *f = *parent;
    }
    else {
        f->parent = NULL;
    }
}