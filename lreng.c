#include <stdio.h>
#include "dynarr.h"
#include "tree.h"
#include "errors.h"
#include "objects.h"
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
        exit(OS_ERR_CODE);
    }
    fread(src, 1, fsize, fp);
    fclose(fp);

    dynarr_t token_list = tokenize(src, fsize, is_debug);
    tree_t syntax_tree = tree_parser(token_list, is_debug);

    bigint_t x;
    x = bigint_from_str("0x123456789abcdef");
    printf("%lx\n", 0x123456789abcdef);
    print_bigint(&x);
    puts("");

    free_bigint(&x);
    printf("%lx\n", 1234567890);
    x = bigint_from_str("1234567890");
    print_bigint(&x);
    puts("");

    free_bigint(&x);
    printf("%lx\n", 1000000000000000);
    x = bigint_from_tens_power(16);
    print_bigint(&x);
    puts("");

    free_dynarr(&token_list);
    free_tree(&syntax_tree);
    free(src);
    return 0;
}
