#include "token.h"

/* Pratt Parsing */

/* Pratt parse context */

typedef struct pratt_parser_context {
    dynarr_token_t tokens;
    dynarr_token_t output;
    int pos;
} pratt_parser_context_t;

void parse_expr(pratt_parser_context_t* context, int cur_binding_power);

/* Shunting Yard */

dynarr_token_t shunting_yard(const dynarr_token_t tokens);