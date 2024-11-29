#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "objects.h"
#include "frame.h"

inline frame_t*
new_frame(const int entry_index) {
    int i;
    frame_t* f = empty_frame();
    /* push top stack */
    push_stack(f, entry_index);
    /* add reserved ids */
    dynarr_t* first_pairs = &((dynarr_t*) f->stack.data)[0];
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        name_obj_pair_t pair = {
            .name = i,
            .object = RESERVED_OBJS[i]
        };
        append(first_pairs, &pair);
    }
    return f;
}

inline frame_t*
empty_frame() {
    frame_t* f = calloc(1, sizeof(frame_t));
    f->entry_indices = new_dynarr(sizeof(int));
    // printf("empty_frame: new entry_indices %p\n", f->entry_indices.data);
    f->stack = new_dynarr(sizeof(dynarr_t));
    // printf("empty_frame: new stack %p\n", f->stack.data);
    f->refer_count = 1;
    return f;
}

/* push new stack_start_index with optional name-object pair */
inline void
push_stack(frame_t* f, const int entry_index) {
    // printf("push_stack: f=%p, entry_index=%d, depth=%d\n", f, entry_index, f->stack.size);
    append(&f->entry_indices, &entry_index);
    dynarr_t new_pairs = new_dynarr(sizeof(name_obj_pair_t));
    // printf("push_stack: new pairs %p\n", new_pairs.data);
    append(&f->stack, &new_pairs);
}

/* free objects in last pairs and pop the stack */
inline void
pop_stack(frame_t* f) {
    // printf("pop_stack: f=%p depth=%d\n", f, f->stack.size);
    int i;
    if (f->entry_indices.size == 0) {
        return;
    }
    dynarr_t* last_pairs = back(&f->stack);
    // printf("pop_stack: remove pairs %p\n", last_pairs->data);
    for (i = 0; i < last_pairs->size; i++) {
        free_object(&((name_obj_pair_t*) last_pairs->data)[i].object);
    }
    free_dynarr(last_pairs);
    pop(&f->stack);
    pop(&f->entry_indices);
}

/* shallow copy of frame */
frame_t* 
copy_frame(const frame_t* f) {
    int i, j;
    frame_t* clone_frame = empty_frame();
    // printf("copy_frame: from=%p, to=%p\n", f, clone_frame);
    for (i = 0; i < f->stack.size; i++) {
        push_stack(clone_frame, ((int*) f->entry_indices.data)[i]);
        dynarr_t* src_pairs = &((dynarr_t*) f->stack.data)[i];
        dynarr_t* dst_pairs = &((dynarr_t*) clone_frame->stack.data)[i];
        for (j = 0; j < src_pairs->size; j++) {
            name_obj_pair_t src_pair = ((name_obj_pair_t*) src_pairs->data)[j];
            name_obj_pair_t dst_pair = {
                .name = src_pair.name,
                .object = copy_object(&src_pair.object)
            };
            append(dst_pairs, &dst_pair);
        }
    }
    return clone_frame;
}

void free_frame(frame_t* f, const int can_free_pairs) {
    int i, j;
    // printf("free_frame: f=%p\n", f);
    free_dynarr(&f->entry_indices);
    if (can_free_pairs) {
        for (i = 0; i < f->stack.size; i++) {
            // printf("free_frame: free pairs %d\n", i);
            dynarr_t* pairs = &((dynarr_t*) f->stack.data)[i];
            for (j = 0; j < pairs->size; j++) {
                free_object(&((name_obj_pair_t*) pairs->data)[j].object);
            }
            free_dynarr(pairs);
        }
    }
    // printf("free_frame: f=%p free stack\n", f);
    free_dynarr(&f->stack);
}

object_t*
frame_get(const frame_t* f, const int name) {
    // printf("frame_get: frame=%p, name=%d\n", f, name);
    /* search backward */
    int i, j;
    for (i = f->stack.size - 1; i >= 0; i--) {
        dynarr_t* pairs = &((dynarr_t*) f->stack.data)[i];
        for (j = 0; j < pairs->size; j++) {
            // printf(
            //     " comparing with name=%d object=",
            //     ((name_obj_pair_t*) pairs->data)[j].name
            // );
            // print_object(&((name_obj_pair_t*) pairs->data)[j].object);
            // puts("");
            if (name == ((name_obj_pair_t*) pairs->data)[j].name) {
                return &((name_obj_pair_t*) pairs->data)[j].object;
            }
        }
    }
    return NULL;
}

/* return where the object was set */
object_t*
frame_set(frame_t* f, const int name, const object_t* obj) {
    // printf("frame_set: frame=%p, name=%d obj=", f, name);
    // print_object((object_t*) obj); puts("");
    int i;
    object_t* found_object = NULL;
    dynarr_t* last_pairs = back(&f->stack);
    // printf("last_pairs={.data=%p, .size=%d, .elem_size=%d}\n", 
    //     last_pairs->data, last_pairs->size, last_pairs->elem_size);
    for (i = 0; i < last_pairs->size; i++) {
        // printf(
        //     " comparing with name=%d object=",
        //     ((name_obj_pair_t*) last_pairs->data)[i].name
        // );
        // print_object(&((name_obj_pair_t*) last_pairs->data)[i].object);
        // puts("");
        if (name == ((name_obj_pair_t*) last_pairs->data)[i].name) {
            found_object = &((name_obj_pair_t*) last_pairs->data)[i].object;
            break;
        }
    }
    if (found_object == NULL) {
        name_obj_pair_t new_pair = {
            .name = name,
            .object = copy_object(obj)
        };
        append(last_pairs, &new_pair);
        return &((name_obj_pair_t*) back(last_pairs))->object;
    }
    else {
        free_object(found_object);
        *found_object = copy_object(obj);
        return found_object;
    }
}
