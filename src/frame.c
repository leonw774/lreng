#include "frame.h"
#include "utils/arena.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "objects.h"
#include "reserved.h"
#include "token.h"

#define PTR_L20BITS(p) ((void*)(((uintptr_t)p) & 0xfffff))

inline frame_t*
frame_new(const frame_t* parent)
{
    int i;
    frame_t* f = calloc(1, sizeof(frame_t));
    assert(f != NULL);
    if (parent == NULL) {
        /* allocate a globals ourself */
        f->globals = calloc(1, sizeof(dynarr_int_t));
        assert(f->globals != NULL);
        *(f->globals) = dynarr_name_objptr_new();
        f->is_own_globals = 1;
        /* add reserved ids to global frame */
        for (i = 0; i < RESERVED_ID_NUM; i++) {
            name_objptr_t pair = {
                .name = i,
                .objptr = (object_t*)&RESERVED_OBJS[i],
            };
            dynarr_name_objptr_append(f->globals, &pair);
        }
    } else {
        /* copy the globals from parent */
        f->globals = parent->globals;
    }
    f->entry_indexs = dynarr_int_new();
    f->stack_pointers = dynarr_int_new();
    f->stack = dynarr_name_objptr_new();
    f->ref_count = 1;
    return f;
}

/* copy a frame and increase ref count of all objects in stack */
inline frame_t*
frame_copy(const frame_t* f)
{
    int i;
    frame_t* clone_frame = calloc(1, sizeof(frame_t));
    assert(clone_frame != NULL);
    clone_frame->globals = f->globals;
    /* copy the entry_indexs and stack pointers */
    clone_frame->entry_indexs = dynarr_int_copy(&f->entry_indexs);
    clone_frame->stack_pointers = dynarr_int_copy(&f->stack_pointers);
    /* deep copy stack */
    clone_frame->stack = dynarr_name_objptr_copy(&f->stack);
    for (i = 0; i < f->stack.size; i++) {
        object_ref(dynarr_name_objptr_at(&clone_frame->stack, i)->objptr);
    }
    clone_frame->ref_count = 1;
    return clone_frame;
}

/* free the stacks and globals if owned it */
inline void
frame_free(frame_t* f)
{
    int i;
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_free: %p\n", f);
#endif
    if (f->is_own_globals && f->globals) {
        for (i = 0; i < f->globals->size; i++) {
            object_deref(dynarr_name_objptr_at(f->globals, i)->objptr);
        }
        dynarr_name_objptr_free(f->globals);
        free(f->globals);
    }
    dynarr_int_free(&f->entry_indexs);
    dynarr_int_free(&f->stack_pointers);
    for (i = 0; i < f->stack.size; i++) {
        object_deref(dynarr_name_objptr_at(&f->stack, i)->objptr);
    }
    dynarr_name_objptr_free(&f->stack);
}

/* push new stack_start_index */
inline void
frame_call(frame_t* f, int entry_index)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("frame_call: %p\n", f);
#endif
    dynarr_int_append(&f->entry_indexs, &entry_index);
    dynarr_int_append(&f->stack_pointers, &(f->stack.size));
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
    stack_start_index = *dynarr_int_back(&f->stack_pointers);
    for (i = f->stack.size; i > stack_start_index; i--) {
        object_deref(dynarr_name_objptr_back(&f->stack)->objptr);
        dynarr_name_objptr_pop(&f->stack);
    }
    dynarr_int_pop(&f->entry_indexs);
    dynarr_int_pop(&f->stack_pointers);
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
        name_objptr_t* pair = dynarr_name_objptr_at(&f->stack, i);
        if (name == pair->name) {
            return pair->objptr;
        }
    }
    /* search in global */
    for (j = 0; j < f->globals->size; j++) {
        name_objptr_t* pair = dynarr_name_objptr_at(f->globals, j);
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
    dynarr_name_objptr_t* target;
    object_t* found_obj = NULL;
    if (f->entry_indexs.size == 0) {
        target = f->globals;
        start = 0;
        end = f->globals->size;
    } else {
        target = &f->stack;
        start = *dynarr_int_back(&f->stack_pointers);
        end = f->stack.size;
    }
    for (i = start; i < end; i++) {
        if (name == (dynarr_name_objptr_at(target, i))->name) {
            found_obj = (dynarr_name_objptr_at(target, i))->objptr;
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
    dynarr_name_objptr_append(target, &new_pair);
    return &(dynarr_name_objptr_back(target)->objptr);
}

int
frame_print(frame_t* f)
{
    int i = 0;
    int printed_bytes_count = 0;
    if (!f) {
        return printf("[Frame NULL]");
    }
    printed_bytes_count = printf(
        "[Frame addr=%p, ref_count=%d, ", PTR_L20BITS(f), f->ref_count
    );
    /* global */
    printed_bytes_count += printf("globals(%d)=[", f->globals->size);
    for (i = 0; i < f->globals->size; i++) {
        name_objptr_t* pair = dynarr_name_objptr_at(f->globals, i);
        if (i != 0) {
            printed_bytes_count += printf(", ");
        }
        printed_bytes_count += printf(
            "(var_id=%d addr=%p type=%s)", pair->name,
            PTR_L20BITS(pair->objptr), OBJ_TYPE_STR[pair->objptr->type]
        );
    }
    printed_bytes_count += printf("], ");
    /* call stack */
    printed_bytes_count += printf("call_stacks(%d)=[", f->entry_indexs.size);
    for (i = 0; i < f->entry_indexs.size; i++) {
        int entry_index = *dynarr_int_at(&f->entry_indexs, i);
        if (i != 0) {
            printed_bytes_count += printf(", ");
        }
        printed_bytes_count += printf("%d", entry_index);
    }
    printed_bytes_count += printf("], ");
    /* stack pointers */
    printed_bytes_count += printf("stack_tops(%d)=[", f->stack_pointers.size);
    for (i = 0; i < f->stack_pointers.size; i++) {
        int sp = *dynarr_int_at(&f->stack_pointers, i);
        if (i != 0) {
            printed_bytes_count += printf(", ");
        }
        printed_bytes_count += printf("%d", sp);
    }
    printed_bytes_count += printf("], ");
    /* stack entrys */
    printed_bytes_count += printf("stack(%d)=[", f->stack.size);
    for (i = 0; i < f->stack.size; i++) {
        name_objptr_t* pair = dynarr_name_objptr_at(&f->stack, i);
        if (i != 0) {
            printed_bytes_count += printf(", ");
        }
        printed_bytes_count += printf(
            "(var_id=%d addr=%p type=%s)", pair->name,
            PTR_L20BITS(pair->objptr), OBJ_TYPE_STR[pair->objptr->type]
        );
    }
    printed_bytes_count += printf("]]");
    fflush(stdout);
    return printed_bytes_count;
}

frame_t*
frame_call_with_closure(const frame_t* caller_frame, const object_t* func)
{
    frame_t* callee_frame = NULL;
    int caller_entry_index = caller_frame->entry_indexs.size == 0
        ? -1
        : *dynarr_int_back(&caller_frame->entry_indexs);

    /* if is direct recursion, copy caller frame except last stack section */
    if (caller_frame->stack.size > 0
        && caller_entry_index == func->as.callable.index) {
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
        int i, is_forked = 0;
        const frame_t* f_init_frame = func->as.callable.init_frame;
        callee_frame = frame_new(f_init_frame);

        /* for every function's init-time frame */
        for (i = 0; i < f_init_frame->entry_indexs.size; i++) {
            int j, start, end;
            const frame_t* src_frame;
            int f_init_index = *dynarr_int_at(&f_init_frame->entry_indexs, i);
            int caller_index = -1;

            /* get caller frame's entry index at this step of closure path */
            if (i < caller_frame->entry_indexs.size) {
                caller_index = *dynarr_int_at(&caller_frame->entry_indexs, i);
            }
            /* push the init-time frame's entry_index to callee */
            dynarr_int_append(&callee_frame->entry_indexs, &f_init_index);
            /* push the current stack size to stack pointers */
            dynarr_int_append(
                &callee_frame->stack_pointers, &callee_frame->stack.size
            );
            /* choose the source frame to copy */
            if (!is_forked && caller_index == f_init_index) {
#ifdef ENABLE_DEBUG_LOG_MORE
                printf("frame_call_with_closure: stack[%d] use caller\n", i);
#endif
                src_frame = caller_frame;
            } else {
#ifdef ENABLE_DEBUG_LOG_MORE
                printf("frame_call_with_closure: stack[%d] use init-time\n", i);
#endif
                src_frame = f_init_frame;
                is_forked = 1;
            }
            /* copy the i-th section of the source frameF's stack to callee */
            start = *dynarr_int_at(&src_frame->stack_pointers, i);
            end = (i + 1 < src_frame->stack_pointers.size)
                ? *dynarr_int_at(&src_frame->stack_pointers, i + 1)
                : src_frame->stack.size;
#ifdef ENABLE_DEBUG_LOG_MORE
            printf("frame_call_with_closure: start,end = %d,%d\n", start, end);
#endif
            for (j = start; j < end; j++) {
                name_objptr_t* pair
                    = dynarr_name_objptr_at(&src_frame->stack, j);
                object_ref(pair->objptr);
                dynarr_name_objptr_append(&callee_frame->stack, pair);
            }
        }
    }
    /* then push the new stack section and entry index to callee frame */
    frame_call(callee_frame, func->as.callable.index);
    return callee_frame;
}
