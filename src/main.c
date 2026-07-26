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
#include <unistd.h>

arena_t token_str_arena;

int global_is_enable_debug_log;
int global_is_compile;

struct fam_str {
    size_t size;
    char data[]; /* flexible array member */
};

struct fam_str*
read_from_file(const char* file_path)
{
    FILE* fp = fopen(file_path, "r");
    size_t fsize;
    struct fam_str* str;
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
    str = malloc(sizeof(struct fam_str) + fsize + 1);
    if (str == NULL) {
        fputs("memory error\n", stderr);
        exit(OS_ERR_CODE);
    }
    str->size = fread(str->data, 1, fsize, fp);
    str->data[fsize] = '\0';
    fclose(fp);
    return str;
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
    fflush(fp);
    return;
}

#define COMPILE_ARGS_LIMIT 128

void
compile(
    syntax_tree_t* syntax_tree, const char* out_file_name,
    char* const addl_cc_args[COMPILE_ARGS_LIMIT], const int addl_cc_args_count
)
{
    const char* codes;
    size_t out_file_name_size = strlen(out_file_name);
    char* transpile_out_file_path;
    char* compile_out_file_path;
    char* cc_args[COMPILE_ARGS_LIMIT + 10];
    int i, j, k;

    /* transpile */
    codes = transpile(syntax_tree);
    transpile_out_file_path = malloc(out_file_name_size + 3);
    strcpy(transpile_out_file_path, out_file_name);
    strcat(transpile_out_file_path, ".c");
    write_to_file(transpile_out_file_path, codes);

    /* compile */
    compile_out_file_path = malloc(out_file_name_size + 5);
    strcpy(compile_out_file_path, out_file_name);
    strcat(compile_out_file_path, ".out");
    i = 0;
    cc_args[i++] = "gcc";
    cc_args[i++] = transpile_out_file_path;
    cc_args[i++] = "-Wall";
    for (j = 0; j < addl_cc_args_count; j++) {
        cc_args[i++] = addl_cc_args[j];
    }
    cc_args[i++] = "-o";
    cc_args[i++] = compile_out_file_path;
    cc_args[i++] = NULL;

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("gcc command:\n");
        for (j = 0; cc_args[j] != NULL; j++) {
            printf("\t%s\n", cc_args[j]);
        }
    }
#endif
    fflush(stdout);

    k = execvp("gcc", cc_args);
    if (k != 0) {
        printf("C compile filed\n");
    }
    free(transpile_out_file_path);
    free(compile_out_file_path);
}

const char* usage
    = "Usage: lreng [OPTION] {file_path}\n"
      "OPTION:\n"
      "\t-d, --debug: output debug to stdout\n"
      "\t-C, --compile[={FILE}]: transpile program to C, output to {FILE}.c, "
      "and compile it to {FILE}.out ({FILE} default is 'a'.)\n"
      "\t-A, --args[={CC ARGUMENT}]: The additional C compiler arguments "
      "other than -Wall.\n";

int
main(int argc, char** argv)
{

    char* in_file_path = NULL;
    char* out_file_name = NULL;
    char* addl_cc_args[COMPILE_ARGS_LIMIT];
    int addl_cc_args_count = 0;
    struct fam_str* input_str;

    int opt;
    struct option long_opts[] = {
        { "debug", no_argument, NULL, 'd' },
        { "compile", optional_argument, NULL, 'C' },
        { "args", required_argument, NULL, 'A' },
        { NULL, 0, NULL, 0 },
    };

    /* init global variables*/
    token_str_arena = (arena_t) {
        .cap = 0,
        .size = 0,
        .ptr = NULL,
    };
    global_is_enable_debug_log = 0;
    global_is_compile = 0;

    /* parse arg */
    while ((opt = getopt_long(argc, argv, "dC::A:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            printf("debug flag is set\n");
            global_is_enable_debug_log = 1;
            break;
        case 'C':
            global_is_compile = 1;
            if (optarg) {
                out_file_name = optarg;
            } else {
                out_file_name = "a";
            }
            break;
        case 'A':
            /* because optind points to next position after the current argv */
            addl_cc_args[addl_cc_args_count] = optarg;
            addl_cc_args_count++;
            addl_cc_args[addl_cc_args_count] = NULL;
            break;
        case '?':
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
    input_str = read_from_file(in_file_path);

    /* start process */
    arena_init(&token_str_arena, input_str->size);
    dynarr_token_t tokens = tokenize(input_str->data, input_str->size);
    syntax_tree_t syntax_tree = syntax_tree_create(tokens);
    /* eval_root(&syntax_tree); */
    if (global_is_compile) {
        compile(&syntax_tree, out_file_name, addl_cc_args, addl_cc_args_count);
    } else {
        eval_root(&syntax_tree);
    }
    syntax_tree_free(&syntax_tree);
    dynarr_token_free(&tokens);
    arena_free(&token_str_arena);

    free(input_str);
#ifdef IS_WASM
    putchar('\n');
#endif
    return 0;
}
