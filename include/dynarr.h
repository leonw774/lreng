#include <stdlib.h>
#include <string.h>

#ifndef DYNARR_H
#define DYNARR_H

/* declaration */

#define DYN_ARR_INIT_CAP 2

typedef struct dynarr {
    unsigned short elem_size; 
    unsigned short size;
    unsigned short cap;
    void* data;
} dynarr_t;

#define dynarr_size sizeof(dynarr_t)

/* definition */

#ifndef MEMCHECK_H

/* create a new empty dynamic array */
static inline dynarr_t
new_dynarr(int elem_size) {
    dynarr_t x;
    x.data = calloc(elem_size, DYN_ARR_INIT_CAP);
    x.elem_size = elem_size;
    x.size = 0;
    x.cap = DYN_ARR_INIT_CAP;
    return x;
};

static inline void
free_dynarr(dynarr_t* x) {
    if (x->data != NULL) {
        free(x->data);
        x->data = NULL;
    } 
    x->size = x->cap = 0;
};

static inline void
reset_dynarr(dynarr_t* x) {
    free_dynarr(x);
    *x = new_dynarr(x->elem_size);
}

static inline dynarr_t
copy_dynarr(const dynarr_t* x) {
    dynarr_t y;
    y = *x;
    y.data = calloc(y.cap, y.elem_size);
    memcpy(y.data, x->data, x->elem_size * x->size);
    return y;
}

/* copy the array as a C-string */
static inline char*
to_str(dynarr_t* x) {
    if (x->data == NULL) return NULL;
    char* arr;
    int arr_sz = x->elem_size * x->size;
    arr = malloc(arr_sz + 1);
    ((char*) arr)[arr_sz] = '\0';
    memcpy(arr, x->data, arr_sz);
    return arr;
}

static inline void
append(dynarr_t* x, const void* const elem) {
    // if (x->data == NULL) return;
    if (x->size == x->cap) {
        x->cap *= 2;
        x->data = realloc(x->data, x->elem_size * x->cap);
    }
    memcpy(x->data + x->elem_size * x->size, elem, x->elem_size);
    x->size++;
};

#endif

static inline void
pop(dynarr_t* x) {
    x->size--;
}

static inline void*
at(const dynarr_t* x, const unsigned int index) {
    return x->data + index * x->elem_size;
}

static inline void*
back(const dynarr_t* x) {
    // if (x->data == NULL) {
    //     return NULL;
    // }
    if (x->size == 0) {
        return NULL;
    }
    return x->data + (x->size - 1) * x->elem_size;
}

/* concat y onto x
   x and y's elem size must be the same
   return 1 if concat seccess, otherwise 0 */
static inline int
concat(dynarr_t* x, dynarr_t* y) {
    if (x->data == NULL || y->data == NULL) {
        return 0;
    }
    if (x->elem_size == y->elem_size) {
        return 0;
    }
    if (x->size + y->size > x->cap) {
        x->cap = x->size + y->size;
        void* tmp_mem = calloc(x->elem_size, x->cap);
        memcpy(tmp_mem, x->data, x->elem_size * x->size);
        free(x->data);
        x->data = tmp_mem;
    }
    int arr_sz = x->size * x->elem_size;
    memcpy(x->data + arr_sz, y->data, arr_sz);
    x->size += y->size;
    return 1;
};

#endif
