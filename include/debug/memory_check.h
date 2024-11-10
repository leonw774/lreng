#include <stdio.h>
#include <stdlib.h>

#ifndef MEM_CHECK_H
#define MEM_CHECK_H

static FILE* MEM_CHECK_OUTFILE;
void* my_malloc(size_t size, const char* file, int line, const char* func);
void* my_calloc(size_t count, size_t size, const char* file, int line, const char* func);

#define malloc(size) my_malloc(size, __FILE__, __LINE__, __FUNCTION__)
#define calloc(count, size) my_calloc(count, size, __FILE__, __LINE__, __FUNCTION__)
#define free(p) my_free(p, __FILE__, __LINE__, __FUNCTION__)


#endif