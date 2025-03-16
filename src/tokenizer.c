#include "lreng.h"
#include "arena.h"
#include "dynarr.h"
#include "errormsg.h"
#include "operators.h"
#include "reserved.h"
#include "token.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* operator constant and tools */

#define COMMENT_CHAR '#'
#define IS_NUM(c) (isdigit(c) || c == '.')
#define IS_ID_HEAD(c) (isalpha(c) || c == '_')
#define IS_ID_BODY(c) (isalnum(c) || c == '_')
#define IS_OP(c) strchr(OP_CHARS, c)
#define IS_HEX(c)                                                              \
    (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f')                  \
     || ((c) >= 'A' && (c) <= 'F'))

#define INVALID_CHAR_MSG                                                       \
    (isprint(c) && !isspace(c) ? "Invalid character: '%c'"                     \
                               : "Invalid character: 0x%x")
#define INVALID_ESCCHAR_MSG                                                    \
    (isprint(c) && !isspace(c) ? "Invalid escape character: '%c'"              \
                               : "Invalid escape character: 0x%x")
#define INVALID_CHAR_LIT_MSG                                                   \
    (isprint(c) && !isspace(c) ? "Expect single quote, get '%c'"               \
                               : "Expect single quote, get: 0x%x")

static inline token_type_enum
get_op_tok_type(char* op_str)
{
    char c = op_str[0];
    int is_left_bracket = 0, is_right_bracket = 0;
    /* if is 2-char operator */
    if (op_str[1] != '\0') {
        return TOK_OP;
    }
    is_left_bracket = c == OP_STRS[OP_LCURLY][0] || c == OP_STRS[OP_LSQUARE][0]
        || c == OP_STRS[OP_LPAREN][0];
    is_right_bracket = c == OP_STRS[OP_RCURLY][0] || c == OP_STRS[OP_RSQUARE][0]
        || c == OP_STRS[OP_RPAREN][0];
    if (is_left_bracket) {
        return TOK_LB;
    } else if (is_right_bracket) {
        return TOK_RB;
    }
    return TOK_OP;
}

/* return the operator name (enum) of an operator str. return -1 if failed */
op_name_enum
get_op_enum(token_t* last_token, char* op_str)
{
    if (op_str[1] == '\0') {
        /* NEG */
        if (last_token == NULL || last_token->type == TOK_OP
            || last_token->type == TOK_LB) {
            if (op_str[0] == '+') {
                return OP_POS;
            } else if (op_str[0] == '-') {
                return OP_NEG;
            }
        }
        /* FCALL */
        if (last_token != NULL
            && (last_token->type == TOK_ID || last_token->type == TOK_RB)) {
            if (op_str[0] == '(') {
                return OP_FCALL;
            }
        }
    }
    int op;
    for (op = 0; op < OPERATOR_COUNT; op++) {
        /* ignore special */
        if (op == OP_FMAKE || op == OP_MMAKE || op == OP_FCALL || op == OP_POS
            || op == OP_NEG) {
            continue;
        }
        if (strcmp(OP_STRS[op], op_str) == 0) {
            return op;
        }
    }
    return -1;
}

/* for updating line, col, and pos */
typedef struct linecol_iterator {
    const char* src;
    int src_len;
    int index;
    linecol_t pos;
} linecol_iterator_t;

/* get char at current position and then increase position by 1 */
char
linecol_read(linecol_iterator_t* const s)
{
    if (s->index == s->src_len) {
        return '\0';
    }
    if (s->src[s->index] == '\n') {
        s->pos.line++;
        s->pos.col = 1;
    } else {
        s->pos.col++;
    }
    char c = s->src[s->index];
    s->index++;
    return c;
}

/* cargo: record the state of token parsing:
   - str: temporary queue
   - tokens: parsed tokens
*/
typedef struct cargo {
    dynarr_t str;
    dynarr_t tokens;
} cargo;

/* append cargo.str it to cargo.tokens, and clear cargo.str */
void
harvest(cargo* cur_cargo, token_type_enum type, linecol_t pos)
{
    char* tok_str = (char*)to_str(&cur_cargo->str);
    if (type == TOK_OP || type == TOK_LB || type == TOK_RB) {
        op_name_enum op
            = get_op_enum((token_t*)back(&cur_cargo->tokens), tok_str);
        free(tok_str);
        if ((int)op == -1) {
            throw_syntax_error(pos, "bad op name");
        }
        /* if is a empty function call, add null */
        token_t* last_token = back(&cur_cargo->tokens);
        if (last_token != NULL && last_token->name == OP_FCALL
            && type == TOK_RB) {
            token_t null_tok = {
                .str = RESERVED_IDS[RESERVED_ID_NAME_NULL],
                .name = RESERVED_ID_NAME_NULL,
                .type = TOK_ID,
                .pos = pos,
            };
            append(&cur_cargo->tokens, &null_tok);
        }
        token_t new_token = { NULL, op, type, pos };
        append(&cur_cargo->tokens, &new_token);
    } else {
        int i, is_in_reserved_ids = 0;
        if (cur_cargo->str.size > MAX_TOKEN_STR_LEN) {
            sprintf(
                ERR_MSG_BUF, "token length cannot exceed %d", MAX_TOKEN_STR_LEN
            );
            throw_syntax_error(pos, ERR_MSG_BUF);
        }
        for (i = 0; i < RESERVED_ID_NUM; i++) {
            if (strcmp(RESERVED_IDS[i], tok_str) == 0) {
                is_in_reserved_ids = 1;
                break;
            }
        }
        if (is_in_reserved_ids) {
            free(tok_str);
            tok_str = (char*)RESERVED_IDS[i];
        } else {
            /* move to token str arena */
            int tok_str_len = strlen(tok_str);
            char* arena_token_str
                = arena_malloc(&token_str_arena, strlen(tok_str) + 1);
            memcpy(arena_token_str, tok_str, tok_str_len);
            free(tok_str);
            tok_str = arena_token_str;
        }
        if (!tok_str) {
            printf("harvest: empty cargo.str\n");
            return;
        }
        token_t new_token = {
            .str = tok_str,
            .name = -1,
            .type = type,
            .pos = pos,
        };
        append(&cur_cargo->tokens, &new_token);
    }
    dynarr_reset(&cur_cargo->str);
}

/* finite state machine: state: input & cargo => state & cargo */
typedef struct state_ret {
    struct state_ret (*state_func)(linecol_iterator_t*, cargo);
    cargo cargo;
} state_ret;

/* declare the states */

/* ignoreds */
state_ret ws_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret comment_state(linecol_iterator_t* pos_iter, cargo cur_cargo);

/* numbers */
state_ret zero_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret num_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret hex_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret bin_state(linecol_iterator_t* pos_iter, cargo cur_cargo);

/* ascii charactors */
state_ret ch_open_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret ch_esc_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret ch_lit_state(linecol_iterator_t* pos_iter, cargo cur_cargo);

state_ret id_state(linecol_iterator_t* pos_iter, cargo cur_cargo);
state_ret op_state(linecol_iterator_t* pos_iter, cargo cur_cargo);

void
print_state_name(state_ret (*state_func)(linecol_iterator_t*, cargo))
{
    if (state_func == &ws_state)
        printf("%-6s", "ws");
    else if (state_func == &comment_state)
        printf("%-6s", "com");
    else if (state_func == &zero_state)
        printf("%-6s", "zero");
    else if (state_func == &num_state)
        printf("%-6s", "num");
    else if (state_func == &hex_state)
        printf("%-6s", "hex");
    else if (state_func == &bin_state)
        printf("%-6s", "bin");
    else if (state_func == &ch_open_state)
        printf("%-6s", "copen");
    else if (state_func == &ch_esc_state)
        printf("%-6s", "cesc");
    else if (state_func == &ch_lit_state)
        printf("%-6s", "clit");
    else if (state_func == &id_state)
        printf("%-6s", "id");
    else if (state_func == &op_state)
        printf("%-6s", "op");
}

/* define the states */

state_ret
ws_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        return (state_ret) { &comment_state, cur_cargo };
    } else if (c == '0') {
        append(&cur_cargo.str, &c);
        return (state_ret) { &zero_state, cur_cargo };
    } else if (IS_NUM(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &num_state, cur_cargo };
    } else if (c == '\'') {
        return (state_ret) { &ch_open_state, cur_cargo };
    } else if (IS_ID_HEAD(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &id_state, cur_cargo };
    } else if (IS_OP(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
comment_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) { NULL, cur_cargo };
    } else if (c == '\n') {
        return (state_ret) { &ws_state, cur_cargo };
    } else {
        return (state_ret) { &comment_state, cur_cargo };
    }
}

state_ret
zero_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_NUM(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &num_state, cur_cargo };
    } else if (c == 'x') {
        append(&cur_cargo.str, &c);
        return (state_ret) { &hex_state, cur_cargo };
    } else if (c == 'b') {
        append(&cur_cargo.str, &c);
        return (state_ret) { &bin_state, cur_cargo };
    } else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
num_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_NUM(c)) {
        if (c == '.' && strchr(cur_cargo.str.data, '.')) {
            throw_syntax_error(pos, "Second decimal dot in number");
        }
        append(&cur_cargo.str, &c);
        return (state_ret) { &num_state, cur_cargo };
    } else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
hex_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_HEX(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &hex_state, cur_cargo };
    } else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
bin_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (c == '0' || c == '1') {
        append(&cur_cargo.str, &c);
        return (state_ret) { &hex_state, cur_cargo };
    } else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
ch_open_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\\') {
        return (state_ret) { &ch_esc_state, cur_cargo };
    }
    if (isprint(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &ch_lit_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
ch_esc_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\'' || c == '\"' || c == '\\') {
        /* c = c; */
    } else if (c == 'a') {
        c = '\a';
    } else if (c == 'b') {
        c = '\b';
    } else if (c == 'n') {
        c = '\n';
    } else if (c == 'r') {
        c = '\r';
    } else if (c == 't') {
        c = '\t';
    } else if (c == 'v') {
        c = '\v';
    } else {
        sprintf(ERR_MSG_BUF, INVALID_ESCCHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
    }
    append(&cur_cargo.str, &c);
    return (state_ret) { &ch_lit_state, cur_cargo };
}

state_ret
ch_lit_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    /* take the ending single quote */
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c != '\'') {
        sprintf(ERR_MSG_BUF, "Expect a single quote, get '%c'", c);
        throw_syntax_error(pos, ERR_MSG_BUF);
    }

    /* transfrom cargo str char into its ascii number */
    unsigned char lit_char = ((char*)cur_cargo.str.data)[0];
    int digit_num = (lit_char < 10) ? 1 : ((lit_char < 100) ? 2 : 3);
    free(cur_cargo.str.data);
    cur_cargo.str.data = calloc(4, sizeof(char));
    cur_cargo.str.size = digit_num;
    cur_cargo.str.cap = 4;
    snprintf(cur_cargo.str.data, 4, "%u", lit_char);
    harvest(&cur_cargo, TOK_NUM, pos);

    /* next state */
    c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_OP(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_LIT_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
id_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_ID, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        harvest(&cur_cargo, TOK_ID, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_ID, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_ID_BODY(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) { &id_state, cur_cargo };
    } else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_ID, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

state_ret
op_state(linecol_iterator_t* pos_iter, cargo cur_cargo)
{
    linecol_t pos = pos_iter->pos;
    char c = linecol_read(pos_iter);
    token_type_enum type;
    if (c == '\0') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        return (state_ret) { NULL, cur_cargo };
    } else if (isspace(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        return (state_ret) { &ws_state, cur_cargo };
    } else if (c == COMMENT_CHAR) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        return (state_ret) { &comment_state, cur_cargo };
    } else if (IS_NUM(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &num_state, cur_cargo };
    } else if (c == '\'') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        return (state_ret) { &ch_open_state, cur_cargo };
    } else if (IS_ID_BODY(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos);
        append(&cur_cargo.str, &c);
        return (state_ret) { &id_state, cur_cargo };
    } else if (IS_OP(c)) {
        if (cur_cargo.str.size == 2
            || (cur_cargo.str.size == 1
                && !is_2char_op(*(char*)at(&cur_cargo.str, 0), c))) {
            /* if is already a 2-char operator or cannot be a 2-char operator */
            type = get_op_tok_type(cur_cargo.str.data);
            harvest(&cur_cargo, type, pos);
        }
        append(&cur_cargo.str, &c);
        return (state_ret) { &op_state, cur_cargo };
    } else {
        sprintf(ERR_MSG_BUF, INVALID_CHAR_MSG, c);
        throw_syntax_error(pos, ERR_MSG_BUF);
        return (state_ret) { NULL, cur_cargo };
    }
}

/* define the state machine: return a dynarr_t of tokens */
dynarr_t
tokenize(const char* src, const unsigned long src_len)
{
    linecol_t pos = { 1, 0 }; /* start at line 1 col 1 */
    linecol_iterator_t pos_iter = { src, src_len, 0, pos };
    cargo cur_cargo;
    state_ret (*state_func)(linecol_iterator_t*, cargo) = ws_state;
    int i, j;
    cur_cargo.tokens = dynarr_new(sizeof(token_t));
    cur_cargo.str = dynarr_new(sizeof(char));

    arena_init(&token_str_arena, src_len);

#ifdef ENABLE_DEBUG_LOG
    int prev_tokens_count = 0;
    if (global_is_enable_debug_log) {
        puts("tokenize");
    }
#endif
    while (1) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            char c = (pos_iter.index < pos_iter.src_len)
                ? pos_iter.src[pos_iter.index]
                : '\0';
            if (isprint(c)) {
                printf("c=\'%c\'\n", c);
            } else {
                printf("c=0x%x\n", c);
            }
        }
#endif
        if (state_func == NULL) {
            break;
        }
        state_ret res = state_func(&pos_iter, cur_cargo);
        cur_cargo = res.cargo;
        state_func = res.state_func;
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            char* tmp_str = to_str(&cur_cargo.str);
            int size = cur_cargo.tokens.size;
            print_state_name(state_func);
            printf("str=\"%s\" ", tmp_str);
            free(tmp_str);
            if (prev_tokens_count != size && size) {
                printf("new_token=");
                token_print(((token_t*)cur_cargo.tokens.data)[size - 1]);
                prev_tokens_count = size;
            }
            puts("");
        }
#endif
    }
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        int i;
        printf("tokens=");
        for (i = 0; i < cur_cargo.tokens.size; i++) {
            token_print(((token_t*)cur_cargo.tokens.data)[i]);
            printf(" ");
        }
        puts("");
    }
#endif
    /* give identifier names:
       init name str map with keywords */
    /* TODO: rewrite this with prefix-tree */
    dynarr_t name_str_map = dynarr_new(sizeof(char*));
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        append(&name_str_map, &RESERVED_IDS[i]);
    }
    for (i = 0; i < cur_cargo.tokens.size; i++) {
        token_t* cur_token = at(&cur_cargo.tokens, i);
        if (cur_token->type != TOK_ID) {
            continue;
        }
        const char* cur_token_str = cur_token->str;
        int given_name = name_str_map.size;
        for (j = 0; j < name_str_map.size; j++) {
            char* str = ((char**)name_str_map.data)[j];
            if (cur_token_str && strcmp(str, cur_token_str) == 0) {
                given_name = j;
                break;
            }
        }
        ((token_t*)cur_cargo.tokens.data)[i].name = given_name;
        if (given_name == name_str_map.size) {
            append(&name_str_map, &cur_token_str);
        }
    }
    dynarr_free(&name_str_map);
    dynarr_free(&cur_cargo.str);
    return cur_cargo.tokens;
}
