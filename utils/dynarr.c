#include "dynarr.h"

#define DYN_ARR_INIT_CAP 4

/* create a new empty dynamic array */
dynarr_t
new_dynarr(int elem_size) {
    dynarr_t x;
    x.data = malloc(elem_size * DYN_ARR_INIT_CAP);
    memset(x.data, 0, elem_size * DYN_ARR_INIT_CAP);
    x.elem_size = elem_size;
    x.size = 0;
    x.cap = DYN_ARR_INIT_CAP;
    return x;
};

void
free_dynarr(dynarr_t* x) {
    if (x->data != NULL && x->size != 0) {
        free(x->data);
        x->size = x->cap = 0;
    } 
};

void
reset_dynarr(dynarr_t* x) {
    free_dynarr(x);
    *x = new_dynarr(x->elem_size);
}

/* copy the array as a C-string */
void*
to_str(dynarr_t* x) {
    if (x->data == NULL) return NULL;
    void* arr;
    int arr_sz = x->elem_size * x->size;
    arr = malloc(arr_sz + 1);
    ((char*) arr)[arr_sz] = '\0';
    memcpy(arr, x->data, arr_sz);
    return arr;
}

void
append(dynarr_t* x, const void* const elem) {
    if (x->data == NULL) return;
    if (x->size == x->cap) {
        x->cap *= 2;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = calloc(x->elem_size, x->cap);
        memcpy(tmp_mem, x->data, x->elem_size * x->size);
        free(x->data);
        x->data = tmp_mem;
    }
    memcpy(x->data + x->elem_size * x->size, elem, x->elem_size);
    x->size += 1;
};

void
pop(dynarr_t* x) {
    x->size--;
}

void*
back(dynarr_t* x) {
    if (x->data == NULL || x->size == 0) {
        return NULL;
    }
    return x->data + (x->size - 1) * x->elem_size;
}

/* concat y onto x
   x and y's elem size must be the same
   return 1 if concat seccess, otherwise 0 */
int
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