#include "token.h"

#ifndef ERRORMSG_H
#define ERRORMSG_H

#define OS_ERR_CODE 1
#define SYNTAX_ERR_CODE 2 
#define SEMANTIC_ERR_CODE 3 
#define RUNTIME_ERR_CODE 4 
#define OTHER_ERR_CODE 5

extern char ERR_MSG_BUF[];
extern void throw_syntax_error(linecol_t pos, const char* msg);
extern void print_semantic_error(linecol_t pos, const char* msg);
extern void print_runtime_error(linecol_t pos, const char* msg);

#endif
