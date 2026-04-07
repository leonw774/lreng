#include "main.h"
#include "objects.h"
#include "reserved.h"
#include "token_tree.h"
#include "utils/arena.h"
#include "utils/errormsg.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

arena_t token_str_arena;

int global_is_enable_debug_log;

int
main(int argc, char** argv)
{
    const char* usage = "Usage: lreng [-d] {file_path}\n";
    char* file_path = NULL;
    FILE* in_fp = NULL;
    unsigned long long fsize = 0;
    char* src = NULL;

    /* init global variables*/
    token_str_arena = (arena_t) {
        .cap = 0,
        .size = 0,
        .ptr = NULL,
    };
    global_is_enable_debug_log = 0;

    /* parse arg */
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

    arena_init(&token_str_arena, fsize);
    dynarr_token_t tokens = tokenize(src, fsize);
    token_tree_t syntax_tree = parse_tokens_to_tree(tokens);
    int is_good_semantic = check_semantic(syntax_tree);
    if (!is_good_semantic) {
        return SEMANTIC_ERR_CODE;
    }
    frame_t* top_frame = frame_new(NULL);
    context_t top_context = {
        .tree = &syntax_tree,
        .cur_frame = top_frame,
        .call_depth = 0,
    };
    object_t* final_result = eval(top_context, syntax_tree.root_index);
    object_deref(final_result);
    frame_free(top_frame);
    free(top_frame);
    token_tree_free(&syntax_tree);
    dynarr_token_free(&tokens);
    arena_free(&token_str_arena);

    free(src);
#ifdef IS_WASM
    putchar('\n');
#endif
    return 0;
}
