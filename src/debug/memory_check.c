#include <stdio.h>
#include <stdlib.h>

#define malloc malloc
#define calloc calloc
#define free free

static FILE* MEM_CHECK_OUTFILE = NULL;

void*
my_malloc(size_t size, const char* file, int line, const char* func) {
    if (MEM_CHECK_OUTFILE == NULL) {
        MEM_CHECK_OUTFILE = fopen("memory_check_out.txt", "w+");
    }
    void* p = malloc(size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "malloc %s:%i::%s %p %li\n",
        file, line, func, p, size
    );
    return p;
}

void*
my_calloc(size_t count, size_t size, const char* file, int line, const char* func) {
    if (MEM_CHECK_OUTFILE == NULL) {
        MEM_CHECK_OUTFILE = fopen("memory_check_out.txt", "w+");
    }
    void* p = calloc(count, size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "calloc %s:%i::%s %p %li\n",
        file, line, func, p, count * size
    );
    return p;
}

void
my_free(void* p, const char* file, int line, const char* func) {
    if (MEM_CHECK_OUTFILE == NULL) {
        MEM_CHECK_OUTFILE = fopen("memory_check_out.txt", "w+");
    }
    free(p);
    fprintf(
        MEM_CHECK_OUTFILE,
        "free %s:%i::%s %p 0\n",
        file, line, func, p
    );
}