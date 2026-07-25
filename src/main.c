#include "eval.h"
#include "objects.h"
#include "reserved.h"
#include "syntax_tree.h"
#include "tokenizer.h"
#include "transpile.h"
#include "utils/arena.h"
#include "utils/errormsg.h"
#include "utils/global_flags.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

arena_t token_str_arena;

int global_is_enable_debug_log;
int global_is_transpile;

struct buf_size {
    char* buf;
    size_t size;
};

struct buf_size
read_from_file(const char* file_path)
{
    FILE* fp = fopen(file_path, "r");
    size_t fsize;
    char* buffer;
    if (fp == NULL) {
        printf("Cannot open file: %s\n", file_path);
        exit(OS_ERR_CODE);
    }
    fseek(fp, 0L, SEEK_END);
    fsize = ftell(fp);
    if (fsize == 0) {
        fputs("file empty\n", stderr);
        exit(0);
    }
    rewind(fp);
    buffer = (char*)malloc(fsize + 1);
    if (buffer == NULL) {
        fputs("memory error\n", stderr);
        exit(OS_ERR_CODE);
    }
    fsize = fread(buffer, 1, fsize, fp);
    buffer[fsize] = '\0';
    fclose(fp);
    return (struct buf_size) {
        .buf = buffer,
        .size = fsize,
    };
}

void
write_to_file(const char* file_path, const char* out)
{
    FILE* fp = fopen(file_path, "w+");
    if (fp == NULL) {
        printf("Cannot open file: %s\n", file_path);
        exit(OS_ERR_CODE);
    }
    fwrite(out, 1, strlen(out), fp);
    return;
}

int
main(int argc, char** argv)
{
    const char* usage
        = "Usage: lreng [OPTION] {file_path}\n"
          "OPTION:\n"
          "-d: output debug info to stdout\n"
          "-C, --compile[={FILE}]: compile program to C and output it to "
          "{FILE} (default: \"out.c\")\n";
    char* in_file_path = NULL;
    char* out_file_path = NULL;
    struct buf_size in_buf_size;

    int opt;
    struct option long_options[] = {
        {
            "compile",
            optional_argument,
            NULL,
            'C',
        },
        {
            NULL,
            0,
            NULL,
            0,
        },
    };

    /* init global variables*/
    token_str_arena = (arena_t) {
        .cap = 0,
        .size = 0,
        .ptr = NULL,
    };
    global_is_enable_debug_log = 0;
    global_is_transpile = 0;

    /* parse arg */
    while ((opt = getopt_long(argc, argv, "C::d", long_options, NULL)) != -1) {
        switch (opt) {
        case 'C':
            global_is_transpile = 1;
            if (optarg) {
                out_file_path = optarg;
            } else {
                out_file_path = "out.c";
            }
            break;
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

    if (optind != argc - 1 || argv[optind] == NULL) {
#ifndef IS_WASM
        puts(usage);
#endif
        return 1;
    } else {
        in_file_path = argv[optind];
    }

    /* read from input */
    in_buf_size = read_from_file(in_file_path);

    /* start process */
    arena_init(&token_str_arena, in_buf_size.size);
    dynarr_token_t tokens = tokenize(in_buf_size.buf, in_buf_size.size);
    syntax_tree_t syntax_tree = syntax_tree_create(tokens);
    /* eval_root(&syntax_tree); */
    if (global_is_transpile) {
        const char* codes = transpile(&syntax_tree);
        write_to_file(out_file_path, codes);
    } else {
        eval_root(&syntax_tree);
    }
    syntax_tree_free(&syntax_tree);
    dynarr_token_free(&tokens);
    arena_free(&token_str_arena);

    free(in_buf_size.buf);
#ifdef IS_WASM
    putchar('\n');
#endif
    return 0;
}
