#include <stdio.h>
#include <stdlib.h>

#ifndef MEM_CHECK_H

#define MEM_CHECK_H

static FILE* MEM_CHECK_OUTFILE;

extern void* mem_check_malloc(
    size_t size,
    const char* file, int line, const char* func
);

extern void* mem_check_calloc(
    size_t count, size_t size,
    const char* file, int line, const char* func
);

#define malloc(size) \
    mem_check_malloc(size, __FILE__, __LINE__, __FUNCTION__)
#define calloc(count, size) \
    mem_check_calloc(count, size, __FILE__, __LINE__, __FUNCTION__)
#define free(p) \
    mem_check_free(p, __FILE__, __LINE__, __FUNCTION__)

typedef struct dynarr dynarr_t;

extern dynarr_t mem_check_new_dynarr(
    int elem_size,
    const char* file, int line, const char* func
);

extern void mem_check_free_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
);

extern void mem_check_reset_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
);

extern char* mem_check_to_str(
    dynarr_t* x,
    const char* file, int line, const char* func
);

extern void mem_check_append(
    dynarr_t* x, const void* const elem,
    const char* file, int line, const char* func
);

#define new_dynarr(elem_size) \
    mem_check_new_dynarr(elem_size, __FILE__, __LINE__, __FUNCTION__)
#define free_dynarr(x) \
    mem_check_free_dynarr(x, __FILE__, __LINE__, __FUNCTION__)
#define reset_dynarr(x) \
    mem_check_reset_dynarr(x, __FILE__, __LINE__, __FUNCTION__)
#define to_str(x) \
    mem_check_to_str(x, __FILE__, __LINE__, __FUNCTION__)
#define append(x, elem) \
    mem_check_append(x, elem, __FILE__, __LINE__, __FUNCTION__)

#endif