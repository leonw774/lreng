#include <stdio.h>
#include "dyn_arr.h"
#include "lreng.h"

int
main(int argc, char** argv) {
    char* filename = NULL;
    int is_debug = 0;
    if (argc == 2) {
        filename = argv[1];
    }
    else if (argc == 3) {
        if (strcmp(argv[2], "-d") != 0) {
            fputs("lreng {filename} [-d]\n", stdout);
            return 0;
        }
        filename = argv[1];
        is_debug = 1;
    }
    else {
        fputs("lreng {filename} [-d]\n", stdout);
        return 0;
    }

    FILE* fp = fopen(filename, "r");
    long fsize = 0;
    char* src = NULL;
    fseek(fp, 0L, SEEK_END);
    fsize = ftell(fp);
    if (fsize == 0) {
        fputs("file empty", stderr);
        return 0;
    }
    rewind(fp);
    src = (char*) malloc(sizeof(char) * fsize);
    if (src == NULL) {
        fputs("memory error", stderr);
        exit(1);
    }
    fread(src, 1, fsize, fp);
    fclose(fp);

    dyn_arr token_list = tokenize(src, fsize, is_debug);
    tree root = tree_parser(token_list, is_debug);

    free(src);
    return 0;
}
