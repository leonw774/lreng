#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_H
#define ARENA_H

typedef struct arena {
    unsigned long cap;
    unsigned long size;
    void* ptr;
} arena_t;

#define ARENA_DEFAULT_CAP 256

static inline void
arena_init(arena_t* arena, unsigned long cap)
{
    if (arena->ptr != NULL) {
        printf("init arena->ptr was not NULL: %p\n", arena->ptr);
        exit(1);
    }
    /* printf("cap %ld\n", cap); */
    arena->cap = cap;
    arena->size = 0;
    arena->ptr = calloc(1, cap ? cap : ARENA_DEFAULT_CAP);
};

static inline void*
arena_malloc(arena_t* arena, unsigned long size)
{
    void* res_ptr = arena->ptr + arena->size;
    arena->size += size;
    if (arena->cap <= arena->size) {
        printf("arena_malloc: arena (addr: %p) reach cap\n", arena);
        exit(1);
    }
    return res_ptr;
};

static inline void*
arena_calloc(arena_t* arena, unsigned long size, unsigned long count)
{
    void* res_ptr = arena_malloc(arena, size * count);
    memset(res_ptr, 0, size * count);
    return res_ptr;
};


static inline void
arena_free(arena_t* arena)
{
    free(arena->ptr);
    arena->ptr = NULL;
    arena->cap = arena->size = 0;
};

#endif
