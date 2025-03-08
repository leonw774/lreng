#include "arena.h"

void
arena_init(arena_t* arena, unsigned long cap) {
    if (arena->ptr != NULL) {
        printf("init arena->ptr was not NULL\n");
        exit(1);
    }
    /* printf("cap %ld\n", cap); */
    arena->cap = cap;
    arena->size = 0;
    arena->ptr = calloc(1, cap ? cap : ARENA_DEFAULT_CAP);
};

void*
arena_malloc(arena_t* arena, unsigned long size) {
    void* res_ptr = arena->ptr + arena->size;
    arena->size += size;
    if (arena->cap <= arena->size) {
        printf("arena_malloc: arena (addr: %p) reach cap\n", arena);
        exit(1);
    }
    return res_ptr;
};

void
arena_free(arena_t* arena) {
    free(arena->ptr);
    arena->ptr = NULL;
    arena->cap = arena->size = 0;
};

/* using arenas */

arena_t token_str_arena = (arena_t) {
    .cap = 0,
    .size = 0,
    .ptr = NULL
};