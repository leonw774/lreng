#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "dynarr.h"
#include "tree.h"
#include "errormsg.h"
#include "objects.h"
#include "lreng.h"

int
main(int argc, char** argv) {
    const char* usage = "Usage: lreng [-d] {file_path}\n";
    int is_debug = 0, is_transpile = 0;
    char* file_path = NULL;
    FILE* in_fp = NULL;
    long fsize = 0;
    char* src = NULL;
    
    int opt_c;
    while ((opt_c = getopt(argc, argv, "d")) != -1) {
        switch (opt_c) {
        case 'd':
            is_debug = 1;
            break;
        case '?':
            printf("Unknown option: '-%c'(%d)\n", optopt, optopt);
            puts(usage);
            return 1;
        default:
            abort();
        }
    }
    if (optind != argc - 1 || argv[optind] == NULL) {
        puts(usage);
        return 1;
    }
    else {
        file_path = argv[optind];
    }

    in_fp = fopen(file_path, "r");
    if (in_fp == NULL) {
        printf("Cannot open file: %s\n", file_path);
        return OS_ERR_CODE;
    }
    fseek(in_fp, 0L, SEEK_END);
    fsize = ftell(in_fp);
    if (fsize == 0) {
        fputs("file empty\n", stderr);
        return 0;
    }
    rewind(in_fp);
    src = (char*) malloc(fsize + 1);
    if (src == NULL) {
        fputs("memory error\n", stderr);
        return OS_ERR_CODE;
    }
    size_t fread_result = fread(src, 1, fsize, in_fp);
    src[fsize] = '\0';
    fclose(in_fp);
#ifdef IS_PROFILE
int i;
for (i = 0; i < 10; i++) {
#endif
    dynarr_t tokens = tokenize(src, fsize, is_debug);
    tree_t syntax_tree = tree_parser(tokens, is_debug);
    int is_good_semantic = semantic_checker(syntax_tree, is_debug);
    if (!is_good_semantic) {
        return SEMANTIC_ERR_CODE;
    }
    frame_t* top_frame = new_frame();
    eval_tree(&syntax_tree, top_frame, syntax_tree.root_index, is_debug);
    free_frame(top_frame);
    free(top_frame);
    free_tree(&syntax_tree);
    free_dynarr(&tokens);
#ifdef IS_PROFILE
}
#endif
    free(src);
#ifdef IS_WASM
    putchar('\n');
#endif
    return 0;
}
