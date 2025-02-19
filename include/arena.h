#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef ARENA_H
#define ARENA_H

typedef struct arena {
    unsigned long cap;
    unsigned long size;
    void* ptr;
} arena_t;

#define ARENA_DEFAULT_CAP 256

#define ARENA_DECLARES(name) \
    extern arena_t arena_##name; \
    extern void arena_##name##_init(unsigned long cap); \
    extern void* arena_##name##_malloc(unsigned long add_size); \
    extern void arena_##name##_free();

#define ARENA_DEFINES(name) \
    arena_t arena_##name = { \
        .cap = ARENA_DEFAULT_CAP, \
        .size = 0, \
        .ptr = NULL \
    }; \
    void arena_##name##_init(unsigned long cap) { \
        arena_##name = (arena_t) { \
            .cap = cap, \
            .size = 0, \
            .ptr = calloc(1, cap ? cap : ARENA_DEFAULT_CAP) \
        }; \
    }; \
    void* arena_##name##_malloc(unsigned long size) { \
        void* res_ptr = (arena_##name).ptr + (arena_##name).size; \
        (arena_##name).size += size; \
        if ((arena_##name).cap <= (arena_##name).size) { \
            printf("arena_##name##_malloc: arena reach cap\n"); \
            exit(0); \
        } \
        return res_ptr; \
    }; \
    void arena_##name##_free() { \
        free(arena_##name.ptr); \
        arena_##name.ptr = NULL; \
        arena_##name.cap = arena_##name.size = 0; \
    };

ARENA_DECLARES(token_str)

#endif
