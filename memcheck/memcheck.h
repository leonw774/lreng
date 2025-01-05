#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"

#ifndef MEMCHECK_H

#define MEMCHECK_H

static FILE* MEMCHECK_OUTFILE;

static void* memcheck_malloc(
    size_t size,
    const char* file, int line, const char* func
);

static void* memcheck_calloc(
    size_t count, size_t size,
    const char* file, int line, const char* func
);

#define malloc(size) \
    memcheck_malloc(size, __FILE__, __LINE__, __FUNCTION__)
#define calloc(count, size) \
    memcheck_calloc(count, size, __FILE__, __LINE__, __FUNCTION__)
#define free(p) \
    memcheck_free(p, __FILE__, __LINE__, __FUNCTION__)

typedef struct dynarr dynarr_t;

static dynarr_t memcheck_new_dynarr(
    int elem_size,
    const char* file, int line, const char* func
);

static void memcheck_free_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
);

static void memcheck_reset_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
);

static dynarr_t memcheck_copy_dynarr(
    const dynarr_t* x,
    const char* file, int line, const char* func
);

static char* memcheck_to_str(
    dynarr_t* x,
    const char* file, int line, const char* func
);

static void memcheck_append(
    dynarr_t* x, const void* const elem,
    const char* file, int line, const char* func
);

#define new_dynarr(elem_size) \
    memcheck_new_dynarr(elem_size, __FILE__, __LINE__, __FUNCTION__)
#define free_dynarr(x) \
    memcheck_free_dynarr(x, __FILE__, __LINE__, __FUNCTION__)
#define reset_dynarr(x) \
    memcheck_reset_dynarr(x, __FILE__, __LINE__, __FUNCTION__)
#define copy_dynarr(x) \
    memcheck_copy_dynarr(x, __FILE__, __LINE__, __FUNCTION__)
#define to_str(x) \
    memcheck_to_str(x, __FILE__, __LINE__, __FUNCTION__)
#define append(x, elem) \
    memcheck_append(x, elem, __FILE__, __LINE__, __FUNCTION__)

#undef malloc
#undef calloc
#undef free

static FILE* MEMCHECK_OUTFILE = NULL;

static void
init_memcheck_out_file() {
    if (MEMCHECK_OUTFILE == NULL) {
        /* MEMCHECK_OUTFILE = fopen("memcheck_out.txt", "w+"); */
        MEMCHECK_OUTFILE = stdout;
    }
}

static void*
memcheck_malloc(size_t size, const char* file, int line, const char* func) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][calloc] %s:%i::%s ",
        file, line, func
    );
    void* p = malloc(size);
    fprintf(
        MEMCHECK_OUTFILE,
        "%p %li\n",
        p, size
    );
    fflush(MEMCHECK_OUTFILE);
    return p;
}

static void*
memcheck_calloc(size_t count, size_t size, const char* file, int line, const char* func) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][calloc] %s:%i::%s ",
        file, line, func
    );
    fflush(MEMCHECK_OUTFILE);
    void* p = calloc(count, size);
    fprintf(
        MEMCHECK_OUTFILE,
        "%p %li\n",
        p, count * size
    );
    fflush(MEMCHECK_OUTFILE);
    return p;
}

static void
memcheck_free(void* p, const char* file, int line, const char* func) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][free] %s:%i::%s %p ",
        file, line, func, p
    );
    fflush(MEMCHECK_OUTFILE);
    free(p);
    fprintf(MEMCHECK_OUTFILE, "-\n");
    fflush(MEMCHECK_OUTFILE);
}

static dynarr_t
memcheck_new_dynarr(
    int elem_size,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    dynarr_t x;
    x.data = memcheck_malloc(elem_size * DYN_ARR_INIT_CAP, __FILE__, __LINE__, __FUNCTION__);
    memset(x.data, 0, elem_size * DYN_ARR_INIT_CAP);
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][new_dynarr] %s:%i::%s %p elem_size=%d\n",
        file, line, func, x.data, elem_size
    );
    x.elem_size = elem_size;
    x.size = 0;
    x.cap = DYN_ARR_INIT_CAP;
    return x;
};

static void
memcheck_free_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][free_dynarr] %s:%i::%s %p -\n",
        file, line, func, x->data
    );
    if (x->data != NULL) {
        memcheck_free(x->data, __FILE__, __LINE__, __FUNCTION__);
        x->size = x->cap = 0;
        x->data = NULL;
    } 
};

static void
memcheck_reset_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][reset_dynarr] %s:%i::%s %p -\n",
        file, line, func, x->data
    );
    free_dynarr(x);
    *x = new_dynarr(x->elem_size);
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][reset_dynarr] %s:%i::%s %p -\n",
        file, line, func, x->data
    );
}

static dynarr_t
memcheck_copy_dynarr(
    const dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][copy_dynarr] %s:%i::%s %p %s\n",
        file, line, func, x->data, x->data
    );
    dynarr_t y;
    y = *x;
    y.data = memcheck_calloc(y.cap, y.elem_size, file, line, func);
    memcpy(y.data, x->data, x->elem_size * x->size);
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][copy_dynarr] %s:%i::%s %p -\n",
        file, line, func, x->data
    );
    return y;
}

/* copy the array as a C-string */
static char*
memcheck_to_str(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    if (x->data == NULL) return NULL;
    char* arr;
    int arr_sz = x->elem_size * x->size;
    arr = memcheck_malloc(arr_sz + 1, __FILE__, __LINE__, __FUNCTION__);
    ((char*) arr)[arr_sz] = '\0';
    memcpy(arr, x->data, arr_sz);
    fprintf(
        MEMCHECK_OUTFILE,
        "\n[memcheck][to_str] %s:%i::%s %p -\n",
        file, line, func, x->data
    );
    return arr;
}

static void
memcheck_append(
    dynarr_t* x, const void* const elem,
    const char* file, int line, const char* func
) {
    init_memcheck_out_file();
    if (x->data == NULL) return;
    if (x->size == x->cap) {
        x->cap *= 2;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = memcheck_calloc(x->elem_size, x->cap, __FILE__, __LINE__, __FUNCTION__);
        fprintf(
            MEMCHECK_OUTFILE,
            "\n[memcheck][append] %s:%i::%s %p calloc\n",
            file, line, func, tmp_mem, tmp_mem
        );
        memcpy(tmp_mem, x->data, x->elem_size * x->size);
        fprintf(
            MEMCHECK_OUTFILE,
            "\n[memcheck][append] %s:%i::%s %p free\n",
            file, line, func, x->data
        );
        memcheck_free(x->data, __FILE__, __LINE__, __FUNCTION__);
        x->data = tmp_mem;
    }
    memcpy(x->data + x->elem_size * x->size, elem, x->elem_size);
    x->size++;
};

#endif
