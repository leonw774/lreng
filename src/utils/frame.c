#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "objects.h"
#include "frame.h"
#include "reserved.h"

inline frame_t*
new_frame() {
    int i;
    frame_t* f = calloc(1, sizeof(frame_t));
    f->global_pairs = new_dynarr(sizeof(name_objptr_pair_t));
    new_stack(f);
    /* add reserved ids to global frame */
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        name_objptr_pair_t pair = {
            .name = i,
            .objptr = (object_t*) &RESERVED_OBJS[i]
        };
        append(&f->global_pairs, &pair);
    }
    f->ref_count = 1;
    return f;
}

/* deep copy call stack
   shallow copy global pairs */
inline frame_t* 
copy_frame(const frame_t* f) {
    int i, j;
    frame_t* clone_frame = calloc(1, sizeof(frame_t));

    /* shallow copy globals pairs */
    clone_frame->global_pairs = f->global_pairs;

    /* deep copy call stack */
    dynarr_t* src_pairs;
    dynarr_t* dst_pairs;
    new_stack(clone_frame);
    for (i = 0; i < f->stack.size; i++) {
        push_stack(clone_frame, *(int*) at(&f->indexs, i));
        src_pairs = at(&f->stack, i);
        dst_pairs = at(&clone_frame->stack, i);
        for (j = 0; j < src_pairs->size; j++) {
            name_objptr_pair_t src_pair = *(name_objptr_pair_t*) at(src_pairs, j);
            name_objptr_pair_t dst_pair = {
                .name = src_pair.name,
                .objptr = copy_object(src_pair.objptr)
            };
            append(dst_pairs, &dst_pair);
        }
    }
    clone_frame->ref_count = 1;
    return clone_frame;
}

/* free global and clear stack and free pairs */
inline void
free_frame(frame_t* f) {
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("free_frame: %p\n", f);
#endif
    int i;
    for (i = 0; i < f->global_pairs.size; i++) {
        free_object(((name_objptr_pair_t*) at(&f->global_pairs, i))->objptr);
    }
    free_dynarr(&f->global_pairs);
    clear_stack(f, 1);
}

inline void
new_stack(frame_t* f) {
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("new_stack: %p\n", f);
#endif
    f->indexs = new_dynarr(sizeof(int));
    f->stack = new_dynarr(sizeof(dynarr_t));
}

/* push new stack_start_index */
inline void
push_stack(frame_t* f, const int entry_index) {
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("push_stack: %p\n", f);
#endif
    append(&f->indexs, &entry_index);
    dynarr_t new_pairs = new_dynarr(sizeof(name_objptr_pair_t));
    append(&f->stack, &new_pairs);
}

/* free objects in last pairs and pop the stack */
inline void
pop_stack(frame_t* f) {
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("pop_stack: %p\n", f);
#endif
    int i;
    if (f->indexs.size == 0) {
        return;
    }
    dynarr_t* last_pairs = back(&f->stack);
    for (i = 0; i < last_pairs->size; i++) {
        free_object(((name_objptr_pair_t*) last_pairs->data)[i].objptr);
    }
    free_dynarr(last_pairs);
    pop(&f->stack);
    pop(&f->indexs);
}

/* free call stack but not the global */
inline void
clear_stack(frame_t* f, const int can_free_pairs) {
    int i, j;
    free_dynarr(&f->indexs);
    if (can_free_pairs) {
        for (i = 0; i < f->stack.size; i++) {
            dynarr_t* pairs = at(&f->stack, i);
            for (j = 0; j < pairs->size; j++) {
                free_object(((name_objptr_pair_t*) pairs->data)[j].objptr);
            }
            free_dynarr(pairs);
        }
    }
    free_dynarr(&f->stack);
}

inline object_t*
frame_get(const frame_t* f, const int name) {
    /* search stack from top to bottom */
    int i, j;
    for (i = f->stack.size - 1; i >= 0; i--) {
        dynarr_t* pairs = at(&f->stack, i);
        for (j = 0; j < pairs->size; j++) {
            if (name == ((name_objptr_pair_t*) at(pairs, j))->name) {
                return ((name_objptr_pair_t*) at(pairs, j))->objptr;
            }
        }
    }
    /* search in global */
    for (j = 0; j < f->global_pairs.size; j++) {
        if (name == ((name_objptr_pair_t*) at(&f->global_pairs, j))->name) {
            return ((name_objptr_pair_t*) at(&f->global_pairs, j))->objptr;
        }
    }
    return NULL;
}

inline void
frame_set(frame_t* f, const int name, object_t* obj) {
    int i;
    object_t* found_object = NULL;
    dynarr_t* pairs;
    if (f->indexs.size == 0) {
        pairs = &f->global_pairs;
    }
    else {
        pairs = back(&f->stack);
    }
    for (i = 0; i < pairs->size; i++) {
        if (name == ((name_objptr_pair_t*) pairs->data)[i].name) {
            found_object = ((name_objptr_pair_t*) pairs->data)[i].objptr;
            break;
        }
    }
    if (found_object == NULL) {
        name_objptr_pair_t new_pair = {
            .name = name,
            .objptr = copy_object(obj)
        };
        append(pairs, &new_pair);
    }
    else {
        free_object(found_object);
        found_object = copy_object(obj);
    }
}
