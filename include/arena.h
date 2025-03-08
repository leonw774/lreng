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

extern void arena_init(arena_t* arena, unsigned long cap);

extern void* arena_malloc(arena_t* arena, unsigned long size);

extern void arena_free(arena_t* arena);

extern arena_t token_str_arena;

#endif
