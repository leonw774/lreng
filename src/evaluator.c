#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errormsg.h"
#include "tree.h"
#include "builtin_funcs.h"
#include "dynarr.h"
#include "frame.h"
#include "lreng.h"

#define NO_OPRAND -1

void
check_type(
    token_t op_token,
    object_type_enum left_type,
    object_type_enum right_type,
    object_t* left_obj,
    object_t* right_obj
) {
    int left_passed, right_passed;
    if (left_type == NO_OPRAND) {
        left_passed = left_obj == NULL;
    }
    else if (left_type == TYPE_ANY) {
        left_passed = left_obj != NULL;
    }
    else {
        left_passed = left_obj != NULL && left_obj->type == left_type;
    }
    if (right_type == NO_OPRAND) {
        right_passed = right_obj == NULL;
    }
    else if (right_type == TYPE_ANY) {
        right_passed = right_obj != NULL;
    }
    else {
        right_passed = right_obj != NULL && right_obj->type == right_type;
    }
    if (!left_passed || !right_passed) {
        sprintf(
            ERR_MSG_BUF,
            "Bad type for %s operator: expect (%s, %s), get (%s, %s)",
            op_token.str,
            left_type >= OBJECT_TYPE_NUM ? "" : OBJECT_TYPE_STR[left_type],
            right_type >= OBJECT_TYPE_NUM ? "" : OBJECT_TYPE_STR[right_type],
            left_obj == NULL ? "" : OBJECT_TYPE_STR[left_obj->type],
            right_obj == NULL ? "" : OBJECT_TYPE_STR[right_obj->type]
        );
        print_runtime_error(op_token.pos.line, op_token.pos.col, ERR_MSG_BUF);
    }
}

object_or_error_t
exec_call(object_t* func_obj, object_t* arg_obj, const int is_debug) {
    /* if is builtin */
    if (func_obj->data.func.builtin_name != -1) {
        object_or_error_t (*func_ptr)(object_t*) =
            BUILDTIN_FUNC_ARRAY[func_obj->data.func.builtin_name];
        if (func_ptr == NULL) {
            return ERR_OBJERR();
        }
        return func_ptr(arg_obj);
    }

    /* push new frame */
    frame_t* f = new_frame(
        func_obj->data.func.frame,
        arg_obj,
        func_obj->data.func.arg_name
    );
    printf("exec_call: push new frame=%p\n", &f);
    /* eval from function root index */
    object_or_error_t res = eval_tree(
        func_obj->data.func.tree,
        func_obj->data.func.entry_index,
        f,
        is_debug
    );
    /* pop frame */
    pop_frame(f);
    return res;
}

object_or_error_t
exec_op(
    token_t op_token,
    object_t* left_obj,
    object_t* right_obj,
    int is_debug
) {
    object_t tmp_obj;
    number_t tmp_number;
    int tmp_bool;
    switch(op_token.name) {
    case OP_FCALL:
        check_type(op_token, TYPE_FUNC, TYPE_ANY, left_obj, right_obj);
        return exec_call(left_obj, right_obj, is_debug);
    /* case OP_POS: */
        /* OP_POS would be discarded in tree parser */
    case OP_NEG:
        check_type(op_token, TYPE_NUMBER, NO_OPRAND, left_obj, right_obj);
        tmp_obj = copy_object(left_obj);
        tmp_obj.data.number.sign = !tmp_obj.data.number.sign;
        return OBJ_OBJERR(tmp_obj);
    case OP_NOT:
        check_type(op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = to_boolean(left_obj) ? ONE_NUMBER() : ZERO_NUMBER()
            }
        }));
    case OP_GETL:
        check_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj);
        return OBJ_OBJERR(copy_object(left_obj->data.pair.left));
    case OP_GETR:
        check_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj);
        return OBJ_OBJERR(copy_object(left_obj->data.pair.right));
    case OP_EXP:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        if (right_obj->data.number.denom.size != 1
            || right_obj->data.number.denom.digit[0] != 1) {
            print_runtime_error(
                op_token.pos.line,
                op_token.pos.col,
                "Exponent must be integer"
            );
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_exp(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_MUL:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_mul(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_DIV:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        if (right_obj->data.number.zero) {
            print_runtime_error(
                op_token.pos.line,
                op_token.pos.col,
                "Divided by zero"
            );
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_div(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_MOD:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_mod(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_ADD:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_add(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_SUB:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = { .number =
                number_sub(&left_obj->data.number, &right_obj->data.number)
            }
        }));
    case OP_LT:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = number_lt(&left_obj->data.number, &right_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_LE:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = !number_lt(&right_obj->data.number, &left_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_GT:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = number_lt(&right_obj->data.number, &left_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_GE:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = !number_lt(&left_obj->data.number, &right_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_EQ:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = number_eq(&left_obj->data.number, &right_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_NE:
        check_type(op_token, TYPE_NUMBER, TYPE_NUMBER, left_obj, right_obj);
        tmp_bool = !number_eq(&left_obj->data.number, &right_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_AND:
        check_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj);
        tmp_bool = to_boolean(left_obj) && to_boolean(right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_OR:
        check_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj);
        tmp_bool = to_boolean(left_obj) || to_boolean(right_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_PAIR:
        check_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj);
        tmp_obj = (object_t) {
            .type = TYPE_PAIR,
            .data = {.pair = {
                .left = alloc_empty_object(left_obj->type),
                .right = alloc_empty_object(right_obj->type),
            }}
        };
        *tmp_obj.data.pair.left = copy_object(left_obj);
        *tmp_obj.data.pair.right = copy_object(right_obj);
        return OBJ_OBJERR(tmp_obj);
    case OP_FCALLR:
        check_type(op_token, TYPE_FUNC, TYPE_ANY, left_obj, right_obj);
        return exec_call(left_obj, right_obj, is_debug);
    case OP_CONDFCALL:
        check_type(op_token, TYPE_ANY, TYPE_PAIR, left_obj, right_obj);
        /* check inside the pair */
        {
            token_t tmp_op_token = (token_t) {
                .name = OP_CONDFCALL,
                .pos = op_token.pos,
                .type = TOK_OP,
                .str = "the members in the conditional function caller"
            };
            check_type(tmp_op_token, TYPE_FUNC, TYPE_FUNC,
                right_obj->data.pair.left, right_obj->data.pair.right);
        }
        return exec_call(
            to_boolean(left_obj)
                ? right_obj->data.pair.left
                : right_obj->data.pair.right,
            NULL,
            is_debug
        );
    default:
        print_runtime_error(
            op_token.pos.line,
            op_token.pos.col,
            "exec_op: bad op name"
        );
        return ERR_OBJERR();
    }
}


object_or_error_t
eval_tree(
    tree_t* tree,
    const int entry_index,
    const frame_t* cur_frame,
    const int is_debug
) {
    const token_t* global_tokens = tree->tokens.data;
    
    /* init as entry_index because in postfix,
       parent's index is greater children's */
    int obj_table_offset = entry_index;
    unsigned long tree_size = 0;
    tree_preorder_iterator_t tree_iter;
    dynarr_t token_index_stack; /* type: int */
    object_t** obj_table;
    #define _OBJ_TABLE(index) (obj_table[index - obj_table_offset])

    token_t cur_token;
    int cur_index, cur_local_index, left_index, right_index;
    object_t* left_obj, * right_obj;

    int i, is_error = 0;
    object_or_error_t tmp_obj_err;

    if (is_debug) {
        printf("eval_tree\n");
        printf("entry_index=%d cur_frame=%p ", entry_index, cur_frame);
    }

    /* iterate tree to get tree size from the root */
    tree_iter = tree_iter_init(tree, entry_index);
    while (tree_iter_get(&tree_iter)) {
        int cur_index = *(int*) back(&tree_iter.index_stack);
        if (cur_index < obj_table_offset) {
            obj_table_offset = cur_index;
        }
        tree_size++;
        tree_iter_next(&tree_iter);
    }
    if (entry_index - obj_table_offset + 1 != tree_size) {
        printf(
            "\neval_tree: bad tree. %d != %d\n",
            entry_index - obj_table_offset + 1,
            tree_size
        );
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
        exit(OTHER_ERR_CODE);
    }
    if (is_debug) {
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
    }
 
    /* init object table */
    obj_table = calloc(tree_size, sizeof(object_t*));

    /* begin evaluation */
    token_index_stack = new_dynarr(sizeof(int));
    append(&token_index_stack, &entry_index);
    while (token_index_stack.size > 0) {
        cur_index = *(int*) back(&token_index_stack);
        cur_token = global_tokens[cur_index];
        if (is_debug) {
            printf("node %d ", cur_index);
            print_token(cur_token);
            fflush(stdout);
        }
        left_index = tree->lefts[cur_index];
        right_index = tree->rights[cur_index];
        if(is_debug) {
            printf(
                " (local=%d) left=%d right=%d\n",
                cur_index - obj_table_offset, left_index, right_index
            );
            fflush(stdout);
        }
        left_obj = (left_index == -1) ? NULL : _OBJ_TABLE(left_index);
        right_obj = (right_index == -1) ? NULL : _OBJ_TABLE(right_index);

        if (cur_token.type == TOK_OP) {
            /* function definer */
            if (cur_token.name == OP_FDEF) {
                _OBJ_TABLE(cur_index) = alloc_empty_object(TYPE_FUNC);
                fflush(stdout);
                (_OBJ_TABLE(cur_index))->data.func = (func_t) {
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_name = -1,
                    .entry_index = left_index,
                    .tree = tree,
                    .frame = cur_frame
                };
                if (is_debug) {
                    printf(" defined function:");
                    print_object(_OBJ_TABLE(cur_index)); puts("");
                }
            }
            /* argument binder */
            else if (cur_token.name == OP_ARG) {
                if (right_obj == NULL) {
                    append(
                        &token_index_stack,
                        &tree->rights[cur_index]
                    );
                    continue;
                }
                if (right_obj->type != TYPE_FUNC) {
                    print_runtime_error(
                        cur_token.pos.line,
                        cur_token.pos.col,
                        "Right side of argument binder should be function"
                    );
                    is_error = 1;
                    break;
                }
                /* move the right object to current and modify arg_name */
                _OBJ_TABLE(cur_index) = right_obj;
                _OBJ_TABLE(right_index) = NULL;
                right_obj->data.func.arg_name = global_tokens[left_index].name;
            }
            /* assignment */
            else if (cur_token.name == OP_ASSIGN) {
                token_t left_token = global_tokens[left_index];
                if (frame_get(cur_frame, left_token.name) != NULL) {
                    sprintf(
                        ERR_MSG_BUF,
                        "Repeated initialization of identifier '%s'",
                        left_token.str
                    );
                    print_runtime_error(
                        left_token.pos.line,
                        left_token.pos.col,
                        ERR_MSG_BUF
                    );
                    is_error = 1;
                    break;
                }
                if (right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    continue;
                }
                /* set frame for the identifier */
                frame_set((frame_t*) cur_frame, left_token.name, right_obj);
                /* if right is identifier copy */
                if (global_tokens[right_index].type == TOK_ID) {
                    if (is_debug) {
                        printf(" free left child: ");
                        print_object(left_obj); puts("");
                    }
                    object_t* o = alloc_empty_object(right_obj->type);
                    *o = copy_object(right_obj);
                    _OBJ_TABLE(cur_index) = o;
                }
                /* otherwise just move the pointer to current node */
                else {
                    _OBJ_TABLE(cur_index) = right_obj;
                    _OBJ_TABLE(right_index) = NULL;
                }
            }
            /* logic-and or logic-or  */
            else if (
                cur_token.name == OP_CONDAND || cur_token.name == OP_CONDOR
            ) {
                int is_condor = cur_token.name == OP_CONDOR;
                token_t left_token = global_tokens[left_index];
                /* eval left first and eval right conditionally */
                if (left_obj == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* is_left_true | is_condor | is_eval_right
                   F            | F         | F
                   F            | T         | T
                   T            | F         | T
                   T            | T         | F             */
                if (to_boolean(left_obj) != is_condor) {
                    if (right_obj == NULL) {
                        append(&token_index_stack, &right_index);
                        continue;
                    }
                    /* move left to current */
                    _OBJ_TABLE(cur_index) = right_obj;
                    _OBJ_TABLE(right_index) = NULL;
                }
                else {
                    /* move right to current */
                    _OBJ_TABLE(cur_index) = left_obj;
                    _OBJ_TABLE(left_index) = NULL;
                }
            }
            else if (cur_token.name == OP_EXPRSEP) {
                if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* free left, move right to current */
                if (is_debug) {
                    printf(" free left child: ");
                    print_object(left_obj); puts("");
                }
                free_object(left_obj);
                _OBJ_TABLE(left_index) = NULL;
                _OBJ_TABLE(cur_index) = right_obj;
                _OBJ_TABLE(right_index) = NULL;
            }
            /* other operator */
            else {
                /* if is unary */
                if (strchr(UNARY_OPS, cur_token.name) && left_obj == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                else if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* execop will NOT alloc new object */
                tmp_obj_err = exec_op(cur_token, left_obj, right_obj, is_debug);
                if (is_debug) {
                    printf(" exec_op safely returned\n");
                }
                /* free the children */
                if (
                    left_obj != NULL
                    && global_tokens[left_index].type != TOK_ID
                ) {
                    if (is_debug) {
                        printf(" free left child: ");
                        print_object(left_obj); puts("");
                    }
                    free_object(left_obj);
                    free(left_obj);
                    _OBJ_TABLE(left_index) = NULL;
                }
                if (
                    right_obj != NULL
                    && global_tokens[right_index].type != TOK_ID
                ) {
                    if (is_debug) {
                        printf(" free right child: ");
                        print_object(right_obj); puts("");
                    }
                    free_object(right_obj);
                    free(right_obj);
                    _OBJ_TABLE(right_index) = NULL;
                }
                /* end eval if error */
                if (tmp_obj_err.is_error) {
                    is_error = 1;
                    break;
                }
                _OBJ_TABLE(cur_index) =
                    alloc_empty_object(tmp_obj_err.obj.type);
                *_OBJ_TABLE(cur_index) = tmp_obj_err.obj;
            }
        }
        else if (cur_token.type == TOK_ID) {
            object_t* o = frame_get(cur_frame, cur_token.name);
            if (o == NULL) {
                sprintf(
                    ERR_MSG_BUF,
                    "Identifier '%s' used uninitialized",
                    cur_token.str
                );
                print_runtime_error(
                    cur_token.pos.line, cur_token.pos.col, ERR_MSG_BUF
                );
                is_error = 1;
                break;
            }
            /* bollow object in frame because
               they wont be free before function return */
            _OBJ_TABLE(cur_index) = o;
            if (is_debug) {
                printf(
                    " get identifiter '%s' (name=%d) from frame (addr=%p): ",
                    cur_token.str, cur_token.name, o
                );
                print_object(o);
                puts("");
            }
        }
        else if (cur_token.type == TOK_NUM) {
            object_t* o = alloc_empty_object(TYPE_NUMBER);
            o->data.number = number_from_str(cur_token.str);
            _OBJ_TABLE(cur_index) = o;
        }
        else {
            printf("eval_tree: bad token type: %d\n", cur_token.type);
            exit(RUNTIME_ERR_CODE);
        }
        if (is_debug) {
            printf("node %d: eval result: ", cur_index);
            print_object(_OBJ_TABLE(cur_index));
            puts("");
        }
        fflush(stdout);
        pop(&token_index_stack);
    }

    /* prepare return object */
    if (is_error) {
        if (is_debug) {
            printf("eval_tree return with error\n");
        }
        tmp_obj_err = ERR_OBJERR();
    }
    else {
        tmp_obj_err = OBJ_OBJERR(copy_object(
            _OBJ_TABLE(entry_index)
        ));
    }

    /* free things */
    free_dynarr(&token_index_stack);
    free_tree_iter(&tree_iter);
    if (is_debug) {
        printf("free obj_tables\n");
        fflush(stdout);
    }
    for (i = 0; i < tree_size; i++) {
        token_type_enum token_type = global_tokens[i + obj_table_offset].type;
        if (token_type != TOK_ID && obj_table[i]) {
            if (is_debug) {
                printf("free [%d] %p\n", i, obj_table[i]);
                fflush(stdout);
            }
            free_object(obj_table[i]);
            free(obj_table[i]);
        }
    }
    free(obj_table);
    if (is_debug) {
        printf("eval_tree returned ");
        print_object(&tmp_obj_err.obj); puts("");
    }
    return tmp_obj_err;
}
