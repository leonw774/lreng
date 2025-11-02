#include "frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "objects.h"
#include "reserved.h"
#include "token.h"

inline frame_t*
frame_new()
{
    int i;
    frame_t* f = calloc(1, sizeof(frame_t));
    f->global_pairs = dynarr_new(sizeof(name_objptr_t));
    stack_new(f);
    /* add reserved ids to global frame */
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        name_objptr_t pair = {
            .name = i,
            .objptr = (object_t*)&RESERVED_OBJS[i],
        };
        append(&f->global_pairs, &pair);
    }
    f->ref_count = 1;
    return f;
}

/* deep copy call stack
   shallow copy global pairs
*/
inline frame_t*
frame_copy(const frame_t* f)
{
    int i, j;
    frame_t* clone_frame = calloc(1, sizeof(frame_t));

    /* shallow copy globals pairs */
    clone_frame->global_pairs = f->global_pairs;

    /* deep copy call stack */
    dynarr_t* src_pairs;
    dynarr_t* dst_pairs;
    stack_new(clone_frame);
    for (i = 0; i < f->stack.size; i++) {
        stack_push(clone_frame, *(int*)at(&f->indexs, i));
        src_pairs = at(&f->stack, i);
        dst_pairs = at(&clone_frame->stack, i);
        for (j = 0; j < src_pairs->size; j++) {
            name_objptr_t src_pair = *(name_objptr_t*)at(src_pairs, j);
            name_objptr_t dst_pair = {
                .name = src_pair.name,
                .objptr = object_ref(src_pair.objptr),
            };
            append(dst_pairs, &dst_pair);
        }
    }
    clone_frame->ref_count = 1;
    return clone_frame;
}

/* free global and clear stack and free pairs */
inline void
frame_free(frame_t* f)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_free: %p\n", f);
#endif
    int i;
    for (i = 0; i < f->global_pairs.size; i++) {
        object_deref(((name_objptr_t*)at(&f->global_pairs, i))->objptr);
    }
    dynarr_free(&f->global_pairs);
    stack_clear(f, 1);
}

inline void
stack_new(frame_t* f)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("stack_new: %p\n", f);
#endif
    f->indexs = dynarr_new(sizeof(int));
    f->stack = dynarr_new(sizeof(dynarr_t));
}

/* push new stack_start_index */
inline void
stack_push(frame_t* f, const int entry_index)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("stack_push: %p\n", f);
#endif
    append(&f->indexs, &entry_index);
    dynarr_t new_pairs = dynarr_new(sizeof(name_objptr_t));
    append(&f->stack, &new_pairs);
}

/* free objects in last pairs and pop the stack */
inline void
stack_pop(frame_t* f)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("stack_pop: %p\n", f);
#endif
    int i;
    if (f->indexs.size == 0) {
        return;
    }
    dynarr_t* last_pairs = back(&f->stack);
    for (i = 0; i < last_pairs->size; i++) {
        object_deref(((name_objptr_t*)at(last_pairs, i))->objptr);
    }
    dynarr_free(last_pairs);
    pop(&f->stack);
    pop(&f->indexs);
}

/* free call stack but not the global */
inline void
stack_clear(frame_t* f, const int can_free_pairs)
{
    int i, j;
    dynarr_free(&f->indexs);
    if (can_free_pairs) {
        for (i = 0; i < f->stack.size; i++) {
            dynarr_t* pairs = at(&f->stack, i);
            for (j = 0; j < pairs->size; j++) {
                object_deref(((name_objptr_t*)at(pairs, j))->objptr);
            }
            dynarr_free(pairs);
        }
    }
    dynarr_free(&f->stack);
}

inline object_t*
frame_get(const frame_t* f, const int name)
{
    /* search stack from top to bottom */
    int i, j;
    for (i = f->stack.size - 1; i >= 0; i--) {
        dynarr_t* pairs = at(&f->stack, i);
        for (j = 0; j < pairs->size; j++) {
            if (name == ((name_objptr_t*)at(pairs, j))->name) {
                return ((name_objptr_t*)at(pairs, j))->objptr;
            }
        }
    }
    /* search in global */
    for (j = 0; j < f->global_pairs.size; j++) {
        if (name == ((name_objptr_t*)at(&f->global_pairs, j))->name) {
            return ((name_objptr_t*)at(&f->global_pairs, j))->objptr;
        }
    }
    return NULL;
}

inline object_t**
frame_set(frame_t* f, const int name, object_t* obj)
{
    int i;
    object_t* found_obj = NULL;
    dynarr_t* pairs;
    if (f->indexs.size == 0) {
        pairs = &f->global_pairs;
    } else {
        pairs = back(&f->stack);
    }
    for (i = 0; i < pairs->size; i++) {
        if (name == ((name_objptr_t*)at(pairs, i))->name) {
            found_obj = ((name_objptr_t*)at(pairs, i))->objptr;
            break;
        }
    }
    /* found collision: return NULL */
    if (found_obj) {
        return NULL;
    }
    name_objptr_t new_pair = {
        .name = name,
        .objptr = object_ref(obj),
    };
    append(pairs, &new_pair);
    return &((name_objptr_t*)back((const dynarr_t*) pairs))->objptr;
}

int
frame_print(frame_t* f)
{
    int printed_bytes_count = 0;
    if (!f) {
        return printf("[Frame NULL]\n");
    }
    printed_bytes_count = printf(
        "[FRAME %p\n  ref_count=%d\n", (void*)f, f->ref_count
    );

    printed_bytes_count += printf("  globals (%d):\n", f->global_pairs.size);
    for (int i = 0; i < f->global_pairs.size; i++) {
        name_objptr_t* p = (name_objptr_t*)at(&f->global_pairs, i);
        printed_bytes_count += printf(
            "    name=%d obj=%p\n", p->name, (void*)p->objptr
        );
    }

    printed_bytes_count += printf("  entry_indexs (%d):\n  ", f->indexs.size);
    for (int i = 0; i < f->indexs.size; i++) {
        int e = *(int*)at(&f->indexs, i);
        printed_bytes_count += printf("%d, ", e);
    }

    printed_bytes_count += printf("  stack depth=%d\n]\n", f->stack.size);
    return printed_bytes_count;
}

