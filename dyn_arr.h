#include<stdlib.h>
#include<string.h>

#ifndef DYNARR_H
#define DYNARR_H

typedef struct {
    void* data;
    int elem_size; 
    int count;
    int cap;
} dyn_arr;

extern dyn_arr new_dyn_arr(int elem_size);

extern void free_dyn_arr(dyn_arr* x);

extern void reset_dyn_arr(dyn_arr* x);

extern void* to_arr(dyn_arr* x, unsigned char is_str);

extern void append(dyn_arr* x, void* elem);

extern void pop(dyn_arr* x);

extern void* back(dyn_arr* x);

extern int concat(dyn_arr* x, dyn_arr* y);

#endif
