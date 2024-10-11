#include <ctype.h>
#include "dynarr.h"
#include "errors.h"
#include "token.h"
#include "operators.h"

/* operator constant and tools */

#define COMMENT_CHAR '#'
#define IS_NUM(c) (isdigit(c) || c == '.')
#define IS_ID_HEAD(c) (isalpha(c) || c == '_')
#define IS_ID_BODY(c) (isalnum(c) || c == '_')
#define IS_OP(c) strchr(OP_CHARS, c)
#define IS_HEX(c) (((c) >= '0' && (c) <= '9') || \
                  ((c) >= 'a' && (c) <= 'f') || \
                  ((c) >= 'A' && (c) <= 'F'))

token_type_enum
get_op_tok_type(char* op_str) {
    char c = op_str[0];
    if (c == OP_STRS[OP_LBLOCK][0] || c == OP_STRS[OP_LPAREN][0]
        || c == OP_STRS[OP_FCALL][0]) {
        return TOK_LB;
    }
    else if (c == OP_STRS[OP_RBLOCK][0] || c == OP_STRS[OP_RPAREN][0]) {
        return TOK_RB;
    }
    else {
        return TOK_OP;
    }
}

/* return the operator name (enum) of an operator str. return -1 if failed */
op_name_enum
get_op_enum(token_t* last_token, char* op_str) {
    if (op_str[1] == '\0') {
        // POS & NEG
        if (last_token == NULL
            || last_token->type == TOK_OP || last_token->type == TOK_LB) {
            if (op_str[0] == '+') {
                return OP_POS;
            }
            else if (op_str[0] == '-') {
                return OP_NEG;
            }
        }
        // FCALL
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
        if (op == OP_POS || op == OP_NEG || op == OP_FDEF || op == OP_FCALL) {
            continue;
        }
        if (strcmp(OP_STRS[op], op_str) == 0) {
            return op;
        }
    }
    return -1;
}

/* TODO: move this to evaluater */
/* transform binary and hex string into decimal */
void
rebase_to_dec(dynarr_t* str) {
    /* translate hex string to deciaml string */
    /* because we use '0x' and '0b' prefix, set base as 0 */
    double length_ratio;
    char* strptr = (char*) str->data;
    unsigned long number;
    if (strptr[0] == '0' && strptr[1] == 'x') {
        length_ratio = 1.20411998266; /* log(16) / log(10) */
        number = strtol(str->data, NULL, 0);
    }
    else if (strptr[0] == '0' && strptr[1] == 'b') {
        length_ratio = 0.30102999566; /* log(2) / log(10) */
        int i = 2;
        number = 0;
        for (; strptr[i] != '\0'; i++) {
            number <<= 1;
            number |= (strptr[i] == '1');
        }
    }
    else {
        printf("rebase_to_dec: bad str format\n");
        return;
    }
    int strlen_upper_bound = (str->count - 2) * length_ratio + 1;
    /* direct manipulate the data at low level */
    free(str->data);
    str->data = malloc(strlen_upper_bound * sizeof(char));
    str->cap = strlen_upper_bound;
    sprintf(str->data, "%d", number);
    str->count = strlen(str->data);
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
linecol_read(linecol_iterator_t* const s) {
    if (s->index == s->src_len) {
        return '\0';
    }
    if (s->src[s->index] == '\n') {
        s->pos.line += 1;
        s->pos.col = 1;
    }
    else {
        s->pos.col += 1;
    }
    char c = s->src[s->index];
    s->index += 1;
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
harvest(cargo* cur_cargo, token_type_enum type, linecol_t pos) {
    if (type == TOK_OP || type == TOK_LB || type == TOK_RB) {
        op_name_enum op = get_op_enum(
            (token_t*) back(&cur_cargo->tokens),
            cur_cargo->str.data
        );
        if (op == -1) {
            throw_syntax_error(
                pos.line, pos.col,
                "bad op name"
            );
        }
        token_t new_token = {{.op = op}, type, pos};
        append(&cur_cargo->tokens, &new_token);
    }
    else {
        if (cur_cargo->str.count >= 128) {
            throw_syntax_error(
                pos.line, pos.col,
                "token length cannot exceed 127"
            );
        }
        char* tok_str = (char*) to_str(&cur_cargo->str);
        if (!tok_str) {
            printf("harvest: empty cargo.str\n");
            return;
        }
        token_t new_token = {tok_str, type, pos};
        append(&cur_cargo->tokens, &new_token);
    }
    reset_dynarr(&cur_cargo->str);
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
print_state_name(state_ret (*state_func)(linecol_iterator_t*, cargo)) {
    if (state_func == &ws_state) printf("%-8s", "ws");
    else if (state_func == &comment_state) printf("%-8s", "comment");
    else if (state_func == &zero_state) printf("%-8s", "zero");
    else if (state_func == &num_state) printf("%-8s", "num");
    else if (state_func == &hex_state) printf("%-8s", "hex");
    else if (state_func == &bin_state) printf("%-8s", "bin");
    else if (state_func == &ch_open_state) printf("%-8s", "ch_open");
    else if (state_func == &ch_esc_state) printf("%-8s", "ch_esc");
    else if (state_func == &ch_lit_state) printf("%-8s", "ch_lit");
    else if (state_func == &id_state) printf("%-8s", "id");
    else if (state_func == &op_state) printf("%-8s", "op");
}

/* define the states */

state_ret
ws_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (c == '0') {
        append(&cur_cargo.str, &c);
        return (state_ret) {&zero_state, cur_cargo};
    }
    else if (IS_NUM(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&num_state, cur_cargo};
    }
    else if (c == '\'') {
        return (state_ret) {&ch_open_state, cur_cargo};
    }
    else if (IS_ID_HEAD(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&id_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
comment_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) {NULL, cur_cargo};
    }
    else if (c == '\n') {
        return (state_ret) {&ws_state, cur_cargo};
    }
    else {
        return (state_ret) {&comment_state, cur_cargo};
    }
}

state_ret
zero_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_NUM(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&num_state, cur_cargo};
    }
    else if (c == 'x') {
        append(&cur_cargo.str, &c);
        return (state_ret) {&hex_state, cur_cargo};
    }
    else if (c == 'b') {
        append(&cur_cargo.str, &c);
        return (state_ret) {&bin_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
num_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_NUM(c)) {
        if (c == '.' && strchr(cur_cargo.str.data, '.')) {
            throw_syntax_error(
                pos_iter->pos.line,
                pos_iter->pos.col,
                "Second decimal dot in number"
            );
        }
        append(&cur_cargo.str, &c);
        return (state_ret) {&num_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
hex_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_HEX(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&hex_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
bin_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (c == '0' || c == '1') {
        append(&cur_cargo.str, &c);
        return (state_ret) {&hex_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
ch_open_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\\') {
        return (state_ret) {&ch_esc_state, cur_cargo};
    }
    if (isprint(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&ch_lit_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
ch_esc_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\'' || c == '\"' || c == '\\') {
        /* c = c; */
    }
    else if (c == 'n') {
        c = '\n';
    }
    else if (c == 't') {
        c = '\t';
    }
    else if (c == 'r') {
        c = '\r';
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid escape character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
    append(&cur_cargo.str, &c);
    return (state_ret) {&ch_lit_state, cur_cargo};
}

state_ret
ch_lit_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    /* take the ending single quote */
    char c = linecol_read(pos_iter);
    if (c != '\'') {
        sprintf(SYNTAX_ERR_MSG, "Expect a single quote, get '%c'", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }

    /* transfrom cargo str char into its ascii number */
    int lit_char = ((char*) cur_cargo.str.data)[0];
    int digit_num = (lit_char < 10) ? 1 : ((lit_char < 100) ? 2 : 3);
    free(cur_cargo.str.data);
    cur_cargo.str.data = malloc(4 * sizeof(char));
    cur_cargo.str.count = digit_num;
    cur_cargo.str.cap = 4;
    sprintf(cur_cargo.str.data, "%d\0", lit_char);
    harvest(&cur_cargo, TOK_NUM, pos_iter->pos);

    /* next state */
    c = linecol_read(pos_iter);
    if (c == '\0') {
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
id_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_ID, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_ID, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_ID, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_ID_BODY(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&id_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_ID, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

state_ret
op_state(linecol_iterator_t* pos_iter, cargo cur_cargo) {
    char c = linecol_read(pos_iter);
    token_type_enum type;
    if (c == '\0') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos_iter->pos);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos_iter->pos);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos_iter->pos);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (c == '\'') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&ch_open_state, cur_cargo};
    }
    else if (IS_ID_BODY(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, pos_iter->pos);
        append(&cur_cargo.str, &c);
        return (state_ret) {&id_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        if (cur_cargo.str.count == 1
            && !is_2char_op(((char*) cur_cargo.str.data)[0], c)) {
            /* if is not a 2-character operator */
            type = get_op_tok_type(cur_cargo.str.data);
            harvest(&cur_cargo, type, pos_iter->pos);
        }
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(SYNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            pos_iter->pos.line,
            pos_iter->pos.col,
            SYNTAX_ERR_MSG
        );
    }
}

/* define the state machine: return a dynarr_t of tokens */
dynarr_t
tokenize(const char* src, const int src_len, const unsigned char is_debug) {
    linecol_t pos = {1, 0}; /* start at line 1 col 1 */
    linecol_iterator_t pos_iter = {src, src_len, 0, pos};
    cargo cur_cargo;
    cur_cargo.tokens = new_dynarr(sizeof(token_t));
    cur_cargo.str = new_dynarr(sizeof(char));
    state_ret (*state_func)(linecol_iterator_t*, cargo) = ws_state;
    int prev_tokens_count = 0;

    if (is_debug) {
        puts("tokenize");
    }
    while (1) {
        if (is_debug) {
            char c = pos_iter.src[pos_iter.index];
            if (isprint(c)) {
                printf("c=\'%c\'\n", c);
            }
            else {
                printf("c=0x%x\n", c);
            }
        }

        if (state_func == NULL) {
            break;
        }

        state_ret res = state_func(&pos_iter, cur_cargo);
        cur_cargo = res.cargo;
        state_func = res.state_func;

        if (is_debug) {
            print_state_name(state_func);
            char* tmp_str = to_str(&cur_cargo.str);
            printf("str=\"%s\" ", tmp_str);
            free(tmp_str);
            int count = cur_cargo.tokens.count;
            if (prev_tokens_count != count && count) {
                printf("new_token=");
                print_token(
                    ((token_t*) cur_cargo.tokens.data)[count - 1]
                );
                prev_tokens_count = count;
            }
            puts("");
        }
    }
    if (is_debug) {
        printf("tokens=");
        int i;
        for (i = 0; i < cur_cargo.tokens.count; i++) {
            print_token(((token_t*) cur_cargo.tokens.data)[i]);
            printf(" ");
        }
        puts("");
    }

    free_dynarr(&cur_cargo.str);
    return cur_cargo.tokens;
}
