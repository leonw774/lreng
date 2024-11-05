#include <string.h>
#include "token.h"
#include "frame.h"
#include "objects.h"

const frame_t
DEFAULT_FRAME() {
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
new_frame(frame_t* parent, object_t* init_obj, int init_name) {
    frame_t f;
    f.parent = parent;
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
            return ((object_t*) f->objects.data) + i;
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

void
frame_set(frame_t* f, const int name, const object_t* obj) {
    object_t* found_obj = frame_find(f, name);
    if (found_obj == NULL) {
        append(&f->names, &name);
        append(&f->objects, obj);
    }
    else {
        free_object(found_obj);
        memcpy(found_obj, obj, sizeof(object_t));
    }
}

/* free frame and set frame = frame->parent */
void frame_return(frame_t* f) {
    frame_t* parent = f->parent;
    free_dynarr(&f->objects);
    free_dynarr(&f->names);
    *f = *parent;
}