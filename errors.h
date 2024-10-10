#include <stdio.h>
#include <stdlib.h>

#ifndef ERROR_H
#define ERROR_H

extern char SYNTAX_ERR_MSG[];

extern void throw_syntax_error(int line, int col, const char* msg);

#endif
