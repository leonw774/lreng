#include "frame.h"
#include "arena.h"

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
    /* add reserved ids to global frame */
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        name_objptr_t pair = {
            .name = i,
            .objptr = (object_t*)&RESERVED_OBJS[i],
        };
        append(&f->global_pairs, &pair);
    }
    f->entry_indexs = dynarr_new(sizeof(int));
    f->stack_pointers = dynarr_new(sizeof(int));
    f->stack = dynarr_new(sizeof(name_objptr_t));
    f->ref_count = 1;
    return f;
}

/* copy a frame and increase ref count of all objects in stack */
inline frame_t*
frame_copy(const frame_t* f)
{
    int i;
    frame_t* clone_frame = calloc(1, sizeof(frame_t));
    /* shallow copy the globals pairs */
    clone_frame->global_pairs = f->global_pairs;
    /* copy the entry_indexs and stack pointers */
    clone_frame->entry_indexs = dynarr_copy(&f->entry_indexs);
    clone_frame->stack_pointers = dynarr_copy(&f->stack_pointers);
    /* deep copy stack */
    clone_frame->stack = dynarr_copy(&f->stack);
    for (i = 0; i < f->stack.size; i++) {
        object_ref(((name_objptr_t*)at(&clone_frame->stack, i))->objptr);
    }
    clone_frame->ref_count = 1;
    return clone_frame;
}

/* copy a frame with only its global pairs */
inline frame_t*
frame_copy_globals(const frame_t* f)
{
    frame_t* new_frame = calloc(1, sizeof(frame_t));
    /* shallow copy the globals pairs */
    new_frame->global_pairs = f->global_pairs;
    new_frame->entry_indexs = dynarr_new(sizeof(int));
    new_frame->stack_pointers = dynarr_new(sizeof(int));
    new_frame->stack = dynarr_new(sizeof(name_objptr_t));
    new_frame->ref_count = 1;
    new_frame->ref_count = 1;
    return new_frame;
}

/* free globals and free stack */
inline void
frame_free(frame_t* f)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_free: %p\n", f);
#endif
    dynarr_free(&f->global_pairs);
    frame_free_stack(f);
}

/* only free stack but not the globals */
inline void
frame_free_stack(frame_t* f)
{
    int i;
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_free_stack: %p\n", f);
#endif
    dynarr_free(&f->entry_indexs);
    dynarr_free(&f->stack_pointers);
    for (i = 0; i < f->stack.size; i++) {
        object_deref(((name_objptr_t*)at(&f->stack, i))->objptr);
    }
    dynarr_free(&f->stack);
}

/* push new stack_start_index */
inline void
frame_call(frame_t* f, const int entry_index)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_call: %p\n", f);
#endif
    append(&f->entry_indexs, &entry_index);
    append(&f->stack_pointers, &(f->stack.size));
}

/* deref and pop objects in the last section of stack */
inline void
frame_return(frame_t* f)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_return: %p\n", f);
#endif
    int i, stack_start_index;
    if (f->entry_indexs.size == 0) {
        return;
    }
    stack_start_index = *(int*)back(&f->stack_pointers);
    for (i = f->stack.size; i > stack_start_index; i--) {
        object_deref(((name_objptr_t*)back(&f->stack))->objptr);
        pop(&f->stack);
    }
    pop(&f->entry_indexs);
    pop(&f->stack_pointers);
}

inline object_t*
frame_get(const frame_t* f, const int name)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_get: %p\n", f);
#endif
    /* search stack from top to bottom */
    int i, j;
    for (i = f->stack.size - 1; i >= 0; i--) {
        name_objptr_t* pair = at(&f->stack, i);
        if (name == pair->name) {
            return pair->objptr;
        }
    }
    /* search in global */
    for (j = 0; j < f->global_pairs.size; j++) {
        name_objptr_t* pair = at(&f->global_pairs, j);
        if (name == pair->name) {
            return pair->objptr;
        }
    }
    return NULL;
}

inline object_t**
frame_set(frame_t* f, const int name, object_t* obj)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_set: %p\n", f);
#endif
    int i, start, end;
    dynarr_t* set_target_pairs;
    object_t* found_obj = NULL;
    if (f->entry_indexs.size == 0) {
        set_target_pairs = &f->global_pairs;
        start = 0;
        end = f->global_pairs.size;
    } else {
        set_target_pairs = &f->stack;
        start = *(int*)back(&f->stack_pointers);
        end = f->stack.size;
    }
    for (i = start; i < end; i++) {
        if (name == ((name_objptr_t*)at(set_target_pairs, i))->name) {
            found_obj = ((name_objptr_t*)at(set_target_pairs, i))->objptr;
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
    append(set_target_pairs, &new_pair);
    return &((name_objptr_t*)back((const dynarr_t*)set_target_pairs))->objptr;
}

int
frame_print(frame_t* f)
{
    int printed_bytes_count = 0;
    if (!f) {
        return printf("[Frame NULL]\n");
    }
    printed_bytes_count
        = printf("[FRAME %p\n  ref_count=%d\n", (void*)f, f->ref_count);

    printed_bytes_count += printf("  globals (%d):\n", f->global_pairs.size);
    for (int i = 0; i < f->global_pairs.size; i++) {
        name_objptr_t* p = (name_objptr_t*)at(&f->global_pairs, i);
        printed_bytes_count
            += printf("    name=%d obj=%p\n", p->name, (void*)p->objptr);
    }

    printed_bytes_count
        += printf("  entry_indexs (%d):\n  ", f->entry_indexs.size);
    for (int i = 0; i < f->entry_indexs.size; i++) {
        int e = *(int*)at(&f->entry_indexs, i);
        printed_bytes_count += printf("%d, ", e);
    }

    printed_bytes_count
        += printf("  stack pointer (%d):\n  ", f->stack_pointers.size);
    for (int i = 0; i < f->stack_pointers.size; i++) {
        int p = *(int*)at(&f->stack_pointers, i);
        printed_bytes_count += printf("%d, ", p);
    }

    printed_bytes_count += printf("  stack size=%d\n]\n", f->stack.size);
    return printed_bytes_count;
}

frame_t*
frame_call_with_closure(const frame_t* caller_frame, const object_t* func)
{
    frame_t* callee_frame = NULL;
    int caller_last_entry_index = caller_frame->entry_indexs.size == 0
        ? -1
        : *(int*)back(&caller_frame->entry_indexs);

    /* if is direct recursion, copy caller frame except last stack section */
    if (caller_frame->stack.size > 0
        && caller_last_entry_index == func->as.callable.index) {
        callee_frame = frame_copy(caller_frame);
        frame_return(callee_frame);
    }
    /* otherwise, starting from i = 0, if the i-th entry index of the
       function's init-time frame and the i-th entry index of the caller frame
       is the same, then the i-th section of callee frame equals caller frame's
       i-th section. but once the entry_index is different, they are in
       different closure path, the rest of init-time frame stack is used
    */
    else {
        int i, caller_frame_index = -1, init_frame_index = -1, is_forked = 0;
        const frame_t* f_init_frame = func->as.callable.init_frame;
        callee_frame = frame_copy_globals(caller_frame);

        /* for every function's init-time frame */
        for (i = 0; i < f_init_frame->entry_indexs.size; i++) {
            int j, start, end;
            const frame_t* frame_to_copy;

            init_frame_index = *(int*)at(&f_init_frame->entry_indexs, i);

            /* get caller frame's entry index at this step of closure path */
            if (i < caller_frame->entry_indexs.size) {
                caller_frame_index = *(int*)at(&caller_frame->entry_indexs, i);
            } else {
                caller_frame_index = -1;
            }

            /* push the init-time frame's entry_index to callee */
            append(&callee_frame->entry_indexs, &init_frame_index);

            /* choose the source frame to copy */
            if (!is_forked && caller_frame_index == init_frame_index) {
#ifdef ENABLE_DEBUG_LOG_MORE
                printf("frame_call_with_closure: stack[%d] use caller\n", i);
#endif
                frame_to_copy = caller_frame;
            } else {
#ifdef ENABLE_DEBUG_LOG_MORE
                printf("frame_call_with_closure: stack[%d] use init-time\n", i);
#endif
                frame_to_copy = f_init_frame;
                is_forked = 1;
            }
            /* copy the i-th section of the source frame's stack to callee */
            start = *(int*)at(&frame_to_copy->stack_pointers, i);
            end = (i + 1 < frame_to_copy->stack_pointers.size)
                ? *(int*)at(&frame_to_copy->stack_pointers, i)
                : frame_to_copy->stack.size;
            for (j = start; j < end; j++) {
                name_objptr_t* pair = at(&frame_to_copy->stack, j);
                object_ref(pair->objptr);
                append(&callee_frame->stack, pair);
            }
        }
    }
    /* then push the new stack section and entry index to callee frame */
    frame_call(callee_frame, func->as.callable.index);
    return callee_frame;
}
