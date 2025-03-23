#include "lreng.h"
#include "my_arenas.h"
#include "dynarr.h"
#include "errormsg.h"
#include "objects.h"
#include "tree.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int global_is_enable_debug_log = 0;

int
main(int argc, char** argv)
{
    const char* usage = "Usage: lreng [-d] {file_path}\n";
    char* file_path = NULL;
    FILE* in_fp = NULL;
    unsigned long long fsize = 0;
    char* src = NULL;

    {
        int opt_c;
        while ((opt_c = getopt(argc, argv, "d")) != -1) {
            switch (opt_c) {
            case 'd':
                global_is_enable_debug_log = 1;
                break;
            case '?':
                printf("Unknown option: '-%c'(%d)\n", optopt, optopt);
                puts(usage);
                return 1;
            default:
                abort();
            }
        }
    }
    if (optind != argc - 1 || argv[optind] == NULL) {
#ifndef IS_WASM
        puts(usage);
#endif
        return 1;
    } else {
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
    src = (char*)malloc(fsize + 1);
    if (src == NULL) {
        fputs("memory error\n", stderr);
        return OS_ERR_CODE;
    }
    fsize = fread(src, 1, fsize, in_fp);
    src[fsize] = '\0';
    fclose(in_fp);
#ifdef IS_PROFILE
#ifndef PROFILE_REPEAT_NUM
#define PROFILE_REPEAT_NUM 10
#endif
    int i;
    for (i = 0; i < PROFILE_REPEAT_NUM; i++) {
#endif
        arena_init(&token_str_arena, fsize);
        arena_init(&digit_arena, fsize * sizeof(u32));
        dynarr_t tokens = tokenize(src, fsize);
        tree_t syntax_tree = tree_parse(tokens);
        int is_good_semantic = check_semantic(syntax_tree);
        if (!is_good_semantic) {
            return SEMANTIC_ERR_CODE;
        }
        frame_t* top_frame = frame_new();
        context_t top_context = {
            .tree = &syntax_tree,
            .cur_frame = top_frame,
        };
        object_t* final_result = eval(top_context, syntax_tree.root_index);
        object_free(final_result);
        frame_free(top_frame);
        free(top_frame);
        tree_free(&syntax_tree);
        dynarr_free(&tokens);
        arena_free(&token_str_arena);
        arena_free(&digit_arena);
#ifdef IS_PROFILE
    }
#endif
    free(src);
#ifdef IS_WASM
    putchar('\n');
#endif
    return 0;
}
