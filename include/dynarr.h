#ifndef DYNARR_H
#define DYNARR_H

#define DYN_ARR_INIT_CAP 4

typedef struct dynarr {
    void* data;
    int elem_size; 
    int size;
    int cap;
} dynarr_t;

#ifndef MEM_CHECK_H

extern dynarr_t new_dynarr(int elem_size);

extern void free_dynarr(dynarr_t* x);

extern void reset_dynarr(dynarr_t* x);

extern char* to_str(dynarr_t* x);

extern void append(dynarr_t* x, const void* const elem);

#endif

extern void pop(dynarr_t* x);

extern void* back(dynarr_t* x);

extern int concat(dynarr_t* x, dynarr_t* y);

#endif
