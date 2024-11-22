#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"

#define malloc malloc
#define calloc calloc
#define free free

static FILE* MEM_CHECK_OUTFILE = NULL;

void init_mem_check_out_file() {
    if (MEM_CHECK_OUTFILE == NULL) {
        // MEM_CHECK_OUTFILE = fopen("mem_check_out.txt", "w+");
        MEM_CHECK_OUTFILE = stdout;
    }
}

void*
mem_check_malloc(size_t size, const char* file, int line, const char* func) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][calloc] %s:%i::%s ",
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
mem_check_calloc(size_t count, size_t size, const char* file, int line, const char* func) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][calloc] %s:%i::%s ",
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
mem_check_free(void* p, const char* file, int line, const char* func) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][free] %s:%i::%s %p ",
        file, line, func, p
    );
    fflush(MEM_CHECK_OUTFILE);
    free(p);
    fprintf(MEM_CHECK_OUTFILE, "done\n");
    fflush(MEM_CHECK_OUTFILE);
}

dynarr_t
mem_check_new_dynarr(
    int elem_size,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    dynarr_t x;
    x.data = mem_check_malloc(elem_size * DYN_ARR_INIT_CAP, __FILE__, __LINE__, __FUNCTION__);
    memset(x.data, 0, elem_size * DYN_ARR_INIT_CAP);
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][new_dynarr] %s:%i::%s %p elem_size=%d\n",
        file, line, func, x.data, elem_size
    );
    x.elem_size = elem_size;
    x.size = 0;
    x.cap = DYN_ARR_INIT_CAP;
    return x;
};

void
mem_check_free_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][free_dynarr] %s:%i::%s %p 0\n",
        file, line, func, x->data
    );
    if (x->data != NULL) {
        mem_check_free(x->data, __FILE__, __LINE__, __FUNCTION__);
        x->size = x->cap = 0;
        x->data = NULL;
    } 
};

void
mem_check_reset_dynarr(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][reset_dynarr] %s:%i::%s %p %s\n",
        file, line, func, x->data, x->data
    );
    free_dynarr(x);
    *x = new_dynarr(x->elem_size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][reset_dynarr] %s:%i::%s %p 0\n",
        file, line, func, x->data
    );
}

dynarr_t
mem_check_copy_dynarr(
    const dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][copy_dynarr] %s:%i::%s %p %s\n",
        file, line, func, x->data, x->data
    );
    dynarr_t y;
    y = *x;
    y.data = mem_check_calloc(y.cap, y.elem_size, file, line, func);
    memcpy(y.data, x->data, x->elem_size * x->size);
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][copy_dynarr] %s:%i::%s %p 0\n",
        file, line, func, x->data
    );
    return y;
}

/* copy the array as a C-string */
char*
mem_check_to_str(
    dynarr_t* x,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    if (x->data == NULL) return NULL;
    char* arr;
    int arr_sz = x->elem_size * x->size;
    arr = mem_check_malloc(arr_sz + 1, __FILE__, __LINE__, __FUNCTION__);
    ((char*) arr)[arr_sz] = '\0';
    memcpy(arr, x->data, arr_sz);
    fprintf(
        MEM_CHECK_OUTFILE,
        "\n[mem_check][to_str] %s:%i::%s %p 0\n",
        file, line, func, x->data
    );
    return arr;
}

void
mem_check_append(
    dynarr_t* x, const void* const elem,
    const char* file, int line, const char* func
) {
    init_mem_check_out_file();
    if (x->data == NULL) return;
    if (x->size == x->cap) {
        x->cap *= 2;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = mem_check_calloc(x->elem_size, x->cap, __FILE__, __LINE__, __FUNCTION__);
        fprintf(
            MEM_CHECK_OUTFILE,
            "\n[mem_check][append] %s:%i::%s %p calloc\n",
            file, line, func, tmp_mem, tmp_mem
        );
        memcpy(tmp_mem, x->data, x->elem_size * x->size);
        fprintf(
            MEM_CHECK_OUTFILE,
            "\n[mem_check][append] %s:%i::%s %p free\n",
            file, line, func, x->data
        );
        mem_check_free(x->data, __FILE__, __LINE__, __FUNCTION__);
        x->data = tmp_mem;
    }
    memcpy(x->data + x->elem_size * x->size, elem, x->elem_size);
    x->size++;
};
