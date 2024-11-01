#ifndef ERROR_H
#define ERROR_H

#define OS_ERR_CODE 1
#define SYNTAX_ERR_CODE 2 
#define SEMANTIC_ERR_CODE 3 
#define RUNTIME_ERR_CODE 4 
#define OTHER_ERR_CODE 5

extern char SYNTAX_ERR_MSG[];

extern void throw_syntax_error(int line, int col, const char* msg);

#endif
