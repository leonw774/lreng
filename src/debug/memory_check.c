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
    fprintf(
        MEM_CHECK_OUTFILE,
        "calloc %s:%i::%s ",
        file, line, func
    );
    void* p = malloc(size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "%p %li\n",
        p, size
    );
    fflush(MEM_CHECK_OUTFILE);
    return p;
}

void*
my_calloc(size_t count, size_t size, const char* file, int line, const char* func) {
    if (MEM_CHECK_OUTFILE == NULL) {
        MEM_CHECK_OUTFILE = fopen("memory_check_out.txt", "w+");
    }
    fprintf(
        MEM_CHECK_OUTFILE,
        "calloc %s:%i::%s ",
        file, line, func
    );
    fflush(MEM_CHECK_OUTFILE);
    void* p = calloc(count, size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "%p %li\n",
        p, count * size
    );
    fflush(MEM_CHECK_OUTFILE);
    return p;
}

void
my_free(void* p, const char* file, int line, const char* func) {
    if (MEM_CHECK_OUTFILE == NULL) {
        MEM_CHECK_OUTFILE = fopen("memory_check_out.txt", "w+");
    }
    fprintf(
        MEM_CHECK_OUTFILE,
        "free %s:%i::%s %p ",
        file, line, func, p
    );
    fflush(MEM_CHECK_OUTFILE);
    free(p);
    fprintf(MEM_CHECK_OUTFILE, "done\n");
    fflush(MEM_CHECK_OUTFILE);
}