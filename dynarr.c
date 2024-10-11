#include "dynarr.h"

#define DYN_ARR_INIT_CAP 4

/* create a new empty dynamic array */
dynarr_t
new_dynarr(int elem_size) {
    dynarr_t x;
    x.data = malloc(elem_size * DYN_ARR_INIT_CAP);
    memset(x.data, 0, elem_size * DYN_ARR_INIT_CAP);
    x.elem_size = elem_size;
    x.count = 0;
    x.cap = DYN_ARR_INIT_CAP;
    return x;
};

void
free_dynarr(dynarr_t* x) {
    if (x->data != NULL && x->count != 0) {
        free(x->data);
        x->count = x->cap = 0;
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
    arr = malloc(x->elem_size * x->count + 1);
    ((char*) arr)[x->elem_size * x->count] = '\0';
    memcpy(arr, x->data, x->elem_size * x->count);
    return arr;
}

void
append(dynarr_t* x, void* elem) {
    if (x->data == NULL) return;
    if (x->count == x->cap) {
        x->cap *= 2;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = malloc(new_cap_byte_sz);
        memset(tmp_mem, 0, new_cap_byte_sz);
        memcpy(tmp_mem, x->data, x->elem_size * x->count);
        free(x->data);
        x->data = tmp_mem;
    }
    memcpy(x->data + x->count * x->elem_size, elem, x->elem_size);
    x->count += 1;
};

void
pop(dynarr_t* x) {
    x->count--;
}

void*
back(dynarr_t* x) {
    if (x->data == NULL || x->count == 0) {
        return NULL;
    }
    return x->data + (x->count - 1) * x->elem_size;
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
    if (x->count + y->count > x->cap) {
        x->cap = x->count + y->count;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = malloc(new_cap_byte_sz);
        memset(tmp_mem, 0, new_cap_byte_sz);
        memcpy(tmp_mem, x->data, new_cap_byte_sz);
        free(x->data);
        x->data = tmp_mem;
    }
    memcpy(
        x->data + x->count * x->elem_size,
        y->data,
        y->count * y->elem_size
    );
    x->count += y->count;
    return 1;
};