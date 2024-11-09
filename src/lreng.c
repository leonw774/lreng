#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"
#include "tree.h"
#include "errormsg.h"
#include "objects.h"
#include "lreng.h"

int
main(int argc, char** argv) {
    const char* usage = "Usage: lreng {filename} [-d]\n";
    char* filename = NULL;
    FILE* fp = fopen(filename, "r");
    long fsize = 0;
    char* src = NULL;
    int is_debug = 0;

    if (argc == 2) {
        filename = argv[1];
    }
    else if (argc == 3) {
        if (strcmp(argv[2], "-d") != 0) {
            fputs(usage, stdout);
            return 0;
        }
        filename = argv[1];
        is_debug = 1;
    }
    else {
        fputs(usage, stdout);
        return 0;
    }

    fp = fopen(filename, "r");
    fseek(fp, 0L, SEEK_END);
    fsize = ftell(fp);
    if (fsize == 0) {
        fputs("file empty", stderr);
        return 0;
    }
    rewind(fp);
    src = (char*) malloc(fsize);
    if (src == NULL) {
        fputs("memory error", stderr);
        exit(OS_ERR_CODE);
    }
    fread(src, 1, fsize, fp);
    fclose(fp);

    dynarr_t tokens = tokenize(src, fsize, is_debug);
    tree_t syntax_tree = tree_parser(tokens, is_debug);
    int is_good_semantic = semantic_checker(syntax_tree, is_debug);
    if (!is_good_semantic) {
        return SEMANTIC_ERR_CODE;
    }
    frame_t main_frame = DEFAULT_FRAME();
    eval_tree(&syntax_tree, &main_frame, is_debug);
    pop_frame(&main_frame);
    free_tree(&syntax_tree);
    free_dynarr(&tokens);
    free(src);
    return 0;
}
