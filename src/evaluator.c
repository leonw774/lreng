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
check_operand_type(
    token_t op_token,
    object_type_enum l_type,
    object_type_enum r_type,
    object_t* l_obj,
    object_t* r_obj
) {
    if ((l_obj == NULL) || (r_type == NO_OPRAND) != (r_obj == NULL)
        || l_type != l_obj->type || r_type != r_obj->type) {
        sprintf(
            ERR_MSG_BUF,
            "Bad argument type for operator %s: expect (%s, %s), get (%s, %s)",
            op_token.str,
            l_type >= OBJECT_TYPE_NUM ? "" : OBJECT_TYPE_STR[l_type],
            r_type >= OBJECT_TYPE_NUM ? "" : OBJECT_TYPE_STR[r_type],
            l_obj == NULL ? "" : OBJECT_TYPE_STR[l_obj->type],
            r_obj == NULL ? "" : OBJECT_TYPE_STR[r_obj->type]
        );
        print_runtime_error(op_token.pos.line, op_token.pos.col, ERR_MSG_BUF);
    }
}

object_or_error_t
exec_call_builtin(int name, object_t* arg_obj) {
    switch (name) {
    case RESERVED_ID_NAME_INPUT:
        return builtin_func_input(arg_obj);
    case RESERVED_ID_NAME_OUTPUT:
        return builtin_func_output(arg_obj);
    default:
        return ERR_OBJERR();
    }
}

object_or_error_t
exec_call(object_t* func_obj, object_t* arg_obj) {
    object_or_error_t res;
    /* if is builtin */
    if (func_obj->data.func.builtin_name != -1) {
        return exec_call_builtin(func_obj->data.func.builtin_name, arg_obj);
    }

    /* push new frame */
    frame_t f = new_frame(
        func_obj->data.func.frame,
        arg_obj,
        func_obj->data.func.arg_name
    );
    /* eval from function root index */
    res = eval_tree(func_obj->data.func.local_tree, &f, 1);
    /* pop frame */
    pop_frame(&f);
    return res;
}

object_or_error_t
exec_op(token_t op_token, object_t* l_obj, object_t* r_obj) {
    object_t res;
    number_t res_number;
    int tmp_bool;
    switch(op_token.name) {
    /* case OP_POS: */ /* OP_POS would be discarded in tree parser */
    case OP_NEG:
        check_operand_type(op_token, TYPE_NUMBER, NO_OPRAND, l_obj, r_obj);
        res = copy_object(l_obj);
        res.data.number.sign = !res.data.number.sign;
        return OBJ_OBJERR(res);
    case OP_NOT:
        check_operand_type(op_token, TYPE_ANY, NO_OPRAND, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = to_boolean(l_obj) ? ONE_NUMBER() : ZERO_NUMBER()
            }
        }));
    case OP_GETL:
        check_operand_type(op_token, TYPE_PAIR, NO_OPRAND, l_obj, r_obj);
        return OBJ_OBJERR(copy_object(l_obj->data.pair.left));
    case OP_GETR:
        check_operand_type(op_token, TYPE_PAIR, NO_OPRAND, l_obj, r_obj);
        return OBJ_OBJERR(copy_object(l_obj->data.pair.right));
    case OP_EXP:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        if (r_obj->data.number.denom.size != 1
            || r_obj->data.number.denom.digit[0] != 1) {
            print_runtime_error(
                op_token.pos.line,
                op_token.pos.col,
                "Exponent must be integer"
            );
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_exp(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_MUL:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_mul(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_DIV:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_div(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_MOD:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_mod(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_ADD:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_add(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_SUB:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {
                .number = number_sub(&l_obj->data.number, &r_obj->data.number)
            }
        }));
    case OP_LT:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = number_lt(&l_obj->data.number, &r_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_LE:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = !number_lt(&r_obj->data.number, &l_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_GT:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = number_lt(&r_obj->data.number, &l_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_GE:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = !number_lt(&l_obj->data.number, &r_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_EQ:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = number_eq(&l_obj->data.number, &r_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_NE:
        check_operand_type(op_token, TYPE_NUMBER, TYPE_NUMBER, l_obj, r_obj);
        tmp_bool = !number_eq(&l_obj->data.number, &r_obj->data.number);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_AND:
        check_operand_type(op_token, TYPE_ANY, TYPE_ANY, l_obj, r_obj);
        tmp_bool = to_boolean(l_obj) && to_boolean(r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_OR:
        check_operand_type(op_token, TYPE_ANY, TYPE_ANY, l_obj, r_obj);
        tmp_bool = to_boolean(l_obj) || to_boolean(r_obj);
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUMBER,
            .data = {.number = number_from_i32(tmp_bool)}
        }));
    case OP_PAIR:
        check_operand_type(op_token, TYPE_ANY, TYPE_ANY, l_obj, r_obj);
        res = (object_t) {
            .type = TYPE_NUMBER,
            .data = {.pair = {
                .left = alloc_empty_object(l_obj->type),
                .right = alloc_empty_object(r_obj->type),
            }}
        };
        *res.data.pair.left = copy_object(l_obj);
        *res.data.pair.right = copy_object(r_obj);
        return OBJ_OBJERR(res);
    case OP_FCALLR:
    case OP_CONDAND:
    case OP_CONDOR:
    case OP_CONDFCALL:
    case OP_EXPRSEP:
    default:
        print_runtime_error(
            op_token.pos.line,
            op_token.pos.col,
            "exec_op: bad op name"
        );
    }
}

object_or_error_t
eval_tree(
    tree_t* tree,
    const frame_t* cur_frame,
    const unsigned char is_debug
) {
    const token_t* global_tokens = tree->tokens.data;
    
    /* init as root_index because in postfix, parent's index > children's */
    int min_local_index = tree->root_index;
    unsigned long tree_size = 0;
    tree_preorder_iterator_t tree_iter;
    dynarr_t token_index_stack; /* type: int */
    object_t** obj_table;

    token_t cur_token;
    int cur_index, cur_local_index;

    object_or_error_t return_obj_err;

    if (is_debug) {
        printf("eval_tree: root=%d parent_frame=%016x ",
            tree->root_index, cur_frame);
    }

    /* iterate tree to get tree size from the root */
    tree_iter = tree_iter_init(tree);
    while (tree_iter_get(&tree_iter)) {
        int cur_index = *(int*) back(&tree_iter.index_stack);
        if (cur_index < min_local_index) {
            min_local_index = cur_index;
        }
        tree_size++;
        tree_iter_next(&tree_iter);
    }
    if (tree->root_index - min_local_index + 1 != tree_size) {
        printf("eval_tree: bad tree\n");
        exit(OTHER_ERR_CODE);
    }
    if (is_debug) {
        printf("tree_size = %d", tree_size);
    }
 
    /* init object table */
    obj_table = malloc(tree_size * sizeof(object_t*));

    /* begin evaluation */
    token_index_stack = new_dynarr(sizeof(int));
    append(&token_index_stack, &tree->root_index);
    while (cur_index = *(int*) back(&token_index_stack)) {
        cur_token = ((token_t*) global_tokens)[cur_index];
        cur_local_index = cur_index - min_local_index;
        int left_index = tree->lefts[cur_index],
            right_index = tree->rights[cur_index];
        object_t* left_obj_p = obj_table[left_index - min_local_index];
        object_t* right_obj_p = obj_table[right_index - min_local_index];
        if (cur_token.type == TOK_OP) {
            /* function definer */
            if (cur_token.name == OP_FDEF) {
                tree_t* local_tree = tree;
                local_tree->root_index = tree->lefts[cur_index];
                obj_table[cur_local_index] = alloc_empty_object(TYPE_FUNC);
                obj_table[cur_local_index]->data.func = (func_t) {
                    .builtin_name = -1,
                    .arg_name = -1,
                    .local_tree = local_tree,
                    .frame = cur_frame
                };
            }
            /* argument binder */
            else if (cur_token.name == OP_ARG) {
                if (right_obj_p == NULL) {
                    append(
                        &token_index_stack,
                        &tree->rights[cur_index]
                    );
                    continue;
                }
                if (right_obj_p->type != TYPE_FUNC) {
                    print_runtime_error(
                        cur_token.pos.line,
                        cur_token.pos.col,
                        "Right side of argument binder should be function"
                    );
                }
                object_t* o = alloc_empty_object(TYPE_FUNC);
                *o = copy_object(right_obj_p);
                o->data.func.arg_name = global_tokens[left_index].name;
                obj_table[cur_local_index] = o;
            }
            /* assignment */
            else if (cur_token.name == OP_ASSIGN) {
                token_t left_token = global_tokens[left_index];
                if (frame_find(cur_frame, left_token.name) != NULL) {
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
                }

                if (right_obj_p == NULL) {
                    append(&token_index_stack, &right_index);
                    continue;
                }
                frame_set((frame_t*) cur_frame, left_token.name, right_obj_p);
                /* move the object pointer from right to current */
                obj_table[cur_local_index] = right_obj_p;
                obj_table[right_index - min_local_index] = NULL;
            }
            /* logic-and or logic-or  */
            else if (cur_token.name == OP_CONDAND
                || cur_token.name == OP_CONDOR) {
                int is_condor = cur_token.name == OP_CONDOR;
                token_t left_token = global_tokens[left_index];
                /* eval left first and eval right conditionally */
                if (left_obj_p == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* is_left_true | is_condor | is_eval_right
                   F            | F         | F
                   F            | T         | T
                   T            | F         | T
                   T            | T         | F             */
                if (to_boolean(left_obj_p) != is_condor) {
                    if (right_obj_p == NULL) {
                        append(&token_index_stack, &right_index);
                        continue;
                    }
                    obj_table[cur_local_index] = right_obj_p;
                    obj_table[right_index - min_local_index] = NULL;
                }
                else {
                    obj_table[cur_local_index] = left_obj_p;
                    obj_table[left_index - min_local_index] = NULL;
                }
            }
            /* other operator */
            else {
                /* if is unary */
                if (strchr(UNARY_OPS, cur_token.name)) {
                    if (left_obj_p == NULL) {
                        append(&token_index_stack, &left_index);
                        continue;
                    }
                }
                else {
                    if (left_obj_p == NULL && right_obj_p == NULL) {
                        append(&token_index_stack, &right_index);
                        append(&token_index_stack, &left_index);
                        continue;
                    }
                }
                return_obj_err = exec_op(cur_token, left_obj_p, right_obj_p);
                if (return_obj_err.is_error) {
                    return return_obj_err;
                }
                *obj_table[cur_index] = return_obj_err.obj;
            }
            if (is_debug) {
                printf("node id: %d eval result: ", cur_index);
                print_object(obj_table[cur_local_index]);
                puts("");
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
            }
            /* bollow object in frame because
               they wont be free before function return */
            obj_table[cur_local_index] = o;
            if (is_debug) {
                printf("get identifiter '%s' from frame: ", cur_token.str);
                print_object(o);
                puts("");
            }
        }
        else if (cur_token.type == TOK_NUM) {
            object_t* o = alloc_empty_object(TYPE_NUMBER);
            o->data.number = number_from_str(cur_token.str);
            obj_table[min_local_index] = o;
        }
        else {
            printf("eval_tree: bad token type: %d\n", cur_token.type);
            exit(RUNTIME_ERR_CODE);
        }
        pop(&token_index_stack);
    }

    /* copy return object */
    return_obj_err = OBJ_OBJERR(copy_object(
        obj_table[tree->root_index - min_local_index]
    ));

    /* free things */
    free_dynarr(&token_index_stack);
    int i;
    for (i = 0; i < tree_size; i++) {
        if (obj_table[i]) {
            free_object(obj_table[i]);
            free(obj_table[i]);
        }
    }
    return return_obj_err;
}
