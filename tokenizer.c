#include <ctype.h>
#include "dyn_arr.h"
#include "errors.h"
#include "token.h"
#include "operators.h"

char STNTAX_ERR_MSG[64];

/* operator constant and tools */

#define COMMENT_CHAR '#'
#define IS_NUM(c) (isdigit(c) || c == '.')
#define IS_ID_HEAD(c) (isalpha(c) || c == '_')
#define IS_ID_BODY(c) (isalnum(c) || c == '_')
#define IS_OP(c) strchr(OP_CHARS, c)
#define IS_HEX(c) (((c) >= '0' && (c) <= '9') || \
                  ((c) >= 'a' && (c) <= 'f') || \
                  ((c) >= 'A' && (c) <= 'F'))

TOKEN_TYPE get_op_tok_type(char* op_str) {
    char c = op_str[0];
    if (c == OP_LBLOCK || c == OP_LPAREN || c == OP_FCALL) {
        return TOK_LB;
    }
    else if (c == OP_RBLOCK || c == OP_RPAREN) {
        return TOK_RB;
    }
    else {
        return TOK_OP;
    }
}

/* return the operator name (enum) of an operator str. return -1 if failed */
OP_NAME get_op_name(token* last_token, char* op_str) {
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
        if (last_token != NULL
            && (last_token->type == TOK_ID || last_token->type == TOK_RB)) {
            // FCALL
            if (op_str[0] == '(') {
                return OP_FCALL;
            }
        }
    }
    unsigned int op_name = 0;
    for (; op_name < OPERATOR_COUNT; op_name++) {
        if (strcmp(OP_STRS[op_name], op_str) == 0) {
            return op_name;
        }
    }
    return -1;
}


/* transform binary and hex string into decimal */
void rebase_to_dec(dyn_arr* str) {
    /* translate hex string to deciaml string */
    /* because we use '0x' and '0b' prefix, set base as 0 */
    double length_ratio;
    char* strptr = (char *) str->data;
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
typedef struct db_info_ctrl {
    const char* src;
    int src_len;
    int pos;
    db_info db_info;
} db_info_ctrl;

/* get char at current position and then increase position by 1 */
char
get_and_next(db_info_ctrl* const s) {
    if (s->pos == s->src_len) {
        return '\0';
    }
    if (s->src[s->pos] == '\n') {
        s->db_info.line += 1;
        s->db_info.col = 1;
    }
    else {
        s->db_info.col += 1;
    }
    char c = s->src[s->pos];
    s->pos += 1;
    return c;
}

/* cargo: record the state of token parsing:
   - str: temporary queue
   - tokens: parsed tokens
*/
typedef struct cargo {
    dyn_arr str;
    dyn_arr tokens;
} cargo;

/* append cargo.str it to cargo.tokens, and clear cargo.str */
void harvest(cargo* cur_cargo, TOKEN_TYPE type, db_info db_info) {
    if (type == TOK_OP || type == TOK_LB || type == TOK_RB) {
        OP_NAME op_name = get_op_name(
            (token*) at(&cur_cargo->tokens, cur_cargo->tokens.count - 1),
            cur_cargo->str.data
        );
        token new_token = {{.op_name = op_name}, type, db_info};
        append(&cur_cargo->tokens, &new_token);
    }
    else {
        char* tok_str = (char*) to_arr(&cur_cargo->str, 1);
        if (!tok_str) {
            printf("harvest: empty cargo.str\n");
            return;
        }
        token new_token = {tok_str, type, db_info};
        append(&cur_cargo->tokens, &new_token);
    }
    reset_dyn_arr(&cur_cargo->str);
}

/* finite state machine: state: input & cargo => state & cargo */

typedef struct state_ret {
    struct state_ret (*state_func)(db_info_ctrl*, cargo);
    cargo cargo;
} state_ret;


/* declare the states */

/* ignoreds */
state_ret
ws_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
comment_state(db_info_ctrl* cur_db_info, cargo cur_cargo);

/* numbers */
state_ret
zero_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
num_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
hex_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
bin_state(db_info_ctrl* cur_db_info, cargo cur_cargo);

/* ascii charactors */
state_ret
chr_start_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
chr_esc_state(db_info_ctrl* cur_db_info, cargo cur_cargo);
state_ret
chr_lit_state(db_info_ctrl* cur_db_info, cargo cur_cargo);

state_ret
id_state(db_info_ctrl* cur_db_info, cargo cur_cargo);

state_ret
op_state(db_info_ctrl* cur_db_info, cargo cur_cargo);


/* define the state machine */

/* return a dyn_arr of tokens */
dyn_arr
tokenize(const char* src, const int src_len, const int is_debug) {
    db_info db_info = {1, 1}; /* start at line 1 col 1 */
    db_info_ctrl cur_db_info_ctrl = {src, src_len, 0, db_info};
    cargo cur_cargo;
    cur_cargo.tokens = new_dyn_arr(sizeof(token));
    cur_cargo.str = new_dyn_arr(sizeof(char));
    state_ret (*state_func)(db_info_ctrl*, cargo) = ws_state;

    if (is_debug) {
        printf("tokenize\n");
    }
    while (state_func != NULL) {
        if (is_debug) {
            printf("state: %x\t", state_func);
            char c = cur_db_info_ctrl.src[cur_db_info_ctrl.pos];
            if (isprint(c)) {
                printf("c=\'%c\'\t", c);
            }
            else {
                printf("c=0x%x\t", c);
            }
        }

        state_ret res = state_func(&cur_db_info_ctrl, cur_cargo);
        cur_cargo = res.cargo;
        state_func = res.state_func;

        if (is_debug) {
            printf("str=\"%s\"\n", (char*) cur_cargo.str.data);
            printf("tokens=");
            int n;
            for (n = 0; n < cur_cargo.tokens.count; n++) {
                token cur_tok = ((token*) cur_cargo.tokens.data)[n];
                const char* token_str;
                if (cur_tok.type == TOK_OP
                    || cur_tok.type == TOK_LB || cur_tok.type == TOK_RB) {
                    token_str = OP_STRS[cur_tok.payload.op_name];
                }
                else {
                    token_str = cur_tok.payload.str;
                }
                printf("\"%s\" ", token_str);
            }
            printf("\n");
        }
    }
    free_dyn_arr(&cur_cargo.str);
    return cur_cargo.tokens;
}


/* define the states */

state_ret
ws_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
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
        return (state_ret) {&chr_start_state, cur_cargo};
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
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
comment_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
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
zero_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
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
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
num_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_NUM(c)) {
        if (c == '.' && strchr(cur_cargo.str.data, '.')) {
            throw_syntax_error(
                cur_db_info->db_info.line,
                cur_db_info->db_info.col,
                "Second decimal dot in number"
            );
        }
        append(&cur_cargo.str, &c);
        return (state_ret) {&num_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
hex_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\0') {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_HEX(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&hex_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
bin_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\0') {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (c == '0' || c == '1') {
        append(&cur_cargo.str, &c);
        return (state_ret) {&hex_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        rebase_to_dec(&cur_cargo.str);
        harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
chr_start_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\\') {
        return (state_ret) {&chr_esc_state, cur_cargo};
    }
    if (isprint(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&chr_lit_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
chr_esc_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    char esc_char;
    switch (c) {
        case '\'':
        case '\"':
        case '\\':
            esc_char = c;
            break;
        case 'n':
            esc_char = '\n';
            break;
        case 't':
            esc_char = '\t';
            break;
        case 'r':
            esc_char = '\r';
            break;
        default:
            sprintf(STNTAX_ERR_MSG, "Invalid escape character: %c\n", c);
            throw_syntax_error(
                cur_db_info->db_info.line,
                cur_db_info->db_info.col,
                STNTAX_ERR_MSG
            );
    }
    append(&cur_cargo.str, &esc_char);
    return (state_ret) {&chr_lit_state, cur_cargo};
}

state_ret
chr_lit_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    /* take the ending single quote */
    char c = get_and_next(cur_db_info);
    if (c != '\'') {
        sprintf(STNTAX_ERR_MSG, "Expect a single quote, get '%c'", c);
        throw_syntax_error(cur_db_info->db_info.line, cur_db_info->db_info.col, STNTAX_ERR_MSG);
    }

    /* transfrom cargo str char into its ascii number */
    int lit_char = ((char*) cur_cargo.str.data)[0];
    int digit_num = (lit_char < 10) ? 1 : ((lit_char < 100) ? 2 : 3);
    free(cur_cargo.str.data);
    cur_cargo.str.data = malloc(4 * sizeof(char));
    cur_cargo.str.count = digit_num;
    cur_cargo.str.cap = 4;
    sprintf(cur_cargo.str.data, "%d\0", lit_char);
    harvest(&cur_cargo, TOK_NUM, cur_db_info->db_info);

    /* next state */
    c = get_and_next(cur_db_info);
    if (c == '\0') {
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
id_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    if (c == '\0') {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (IS_ID_BODY(c)) {
        append(&cur_cargo.str, &c);
        return (state_ret) {&id_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        harvest(&cur_cargo, TOK_ID, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}

state_ret
op_state(db_info_ctrl* cur_db_info, cargo cur_cargo) {
    char c = get_and_next(cur_db_info);
    TOKEN_TYPE type;
    if (c == '\0') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, cur_db_info->db_info);
        return (state_ret) {NULL, cur_cargo};
    }
    else if (isspace(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, cur_db_info->db_info);
        return (state_ret) {&ws_state, cur_cargo};
    }
    else if (c == COMMENT_CHAR) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, cur_db_info->db_info);
        return (state_ret) {&comment_state, cur_cargo};
    }
    else if (c == '\'') {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&chr_start_state, cur_cargo};
    }
    else if (IS_ID_BODY(c)) {
        type = get_op_tok_type(cur_cargo.str.data);
        harvest(&cur_cargo, type, cur_db_info->db_info);
        append(&cur_cargo.str, &c);
        return (state_ret) {&id_state, cur_cargo};
    }
    else if (IS_OP(c)) {
        if (!is_2char_op(((char*) cur_cargo.str.data)[0], c)) {
            /* if is not a 2-character operator */
            type = get_op_tok_type(cur_cargo.str.data);
            harvest(&cur_cargo, type, cur_db_info->db_info);
        }
        append(&cur_cargo.str, &c);
        return (state_ret) {&op_state, cur_cargo};
    }
    else {
        sprintf(STNTAX_ERR_MSG, "Invalid character: %c\n", c);
        throw_syntax_error(
            cur_db_info->db_info.line,
            cur_db_info->db_info.col,
            STNTAX_ERR_MSG
        );
    }
}
