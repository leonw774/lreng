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

static inline int
is_bad_type(
    token_t op_token,
    int left_type,
    int right_type,
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
        printf("%d", op_token.name);
        sprintf(
            ERR_MSG_BUF,
            "Bad type for operator \"%s\": expect (%s, %s), get (%s, %s)",
            OP_STRS[op_token.name],
            left_type == NO_OPRAND ? "" : OBJECT_TYPE_STR[left_type],
            right_type == NO_OPRAND ? "" : OBJECT_TYPE_STR[right_type],
            left_obj == NULL ? "" : OBJECT_TYPE_STR[left_obj->type],
            right_obj == NULL ? "" : OBJECT_TYPE_STR[right_obj->type]
        );
        print_runtime_error(op_token.pos.line, op_token.pos.col, ERR_MSG_BUF);
        return 1;
    }
    return 0;
}

static inline object_or_error_t
exec_call(
    const tree_t* tree,
    const frame_t* cur_frame,
    object_t* func_obj,
    object_t* arg_obj,
    const int is_debug
) {
    /* if is builtin */
    if (func_obj->data.func.builtin_name != -1) {
        object_or_error_t (*func_ptr)(object_t*) =
            BUILDTIN_FUNC_ARRAY[func_obj->data.func.builtin_name];
        if (func_ptr == NULL) {
            return ERR_OBJERR();
        }
        return func_ptr(arg_obj);
    }
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_call: prepare call frame\n");
    }
#endif
    int i, is_forked = 0, create_index = -1, cur_index = -1;
    frame_t* call_frame = calloc(1, sizeof(frame_t));
    call_frame->global_pairs = cur_frame->global_pairs;
    new_stack(call_frame);
    if (!func_obj->data.func.is_pure) {
        #define func_init_time_frame func_obj->data.func.init_time_frame
        /* if the i-th stack of init-time frame and current frame is the
        same, call-frame's i-th stack = current frame's i-th stack. but once the
        entry_index is different they are in different closure path, so only
        the rest of init-time-frame stack is used */
        for (i = 0; i < func_init_time_frame->entry_indices.size; i++) {
            create_index = *(int*) at(&func_init_time_frame->entry_indices, i);
            cur_index = -1;
            if (i < cur_frame->entry_indices.size) {
                cur_index = *(int*) at(&cur_frame->entry_indices, i);
            }
            /* push the entry_index */
            append(&call_frame->entry_indices, &create_index);

            dynarr_t* src_pairs = NULL;
            if (!is_forked && cur_index == create_index) {
                src_pairs = (dynarr_t*) at(&cur_frame->stack, i);
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_call: stack[%d] use current\n", i);
                }
#endif
            }
            else {
                is_forked = 1;
                src_pairs = (dynarr_t*) at(&func_init_time_frame->stack, i);
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_call: stack[%d] use init-time\n", i);
                }
#endif
            }
            /* push the shallow copy of source pairs */
            append(&call_frame->stack, src_pairs);
        }
        #undef func_init_time_frame
    }

    /* push new stack to call_frame and set argument */
    push_stack(call_frame, func_obj->data.func.entry_index);
    if (func_obj->data.func.arg_name != -1) {
        frame_set(call_frame, func_obj->data.func.arg_name, arg_obj);
    }
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_call: call_frame=%p\nfunc_obj=", call_frame);
        print_object(func_obj, '\n');
        printf("arg_obj=");
        print_object(arg_obj, '\n');
    }
#endif
    /* eval from function's entry index */
    object_or_error_t res = eval_tree(
        tree,
        call_frame,
        func_obj->data.func.entry_index,
        is_debug
    );
    /* free the object own by this call */
    pop_stack(call_frame);
    /* free the rest of stack but not free bollowed pairs */
    clear_stack(call_frame, 0);
    free(call_frame);
    return res;
}

static inline object_or_error_t
exec_op(
    const tree_t* tree,
    const frame_t* cur_frame,
    token_t op_token,
    object_t* left_obj,
    object_t* right_obj,
    const int is_debug
) {
    object_t tmp_obj;
    switch(op_token.name) {
    case OP_FCALL:
        if (is_bad_type(op_token, TYPE_FUNC, TYPE_ANY, left_obj, right_obj)) {
            return ERR_OBJERR();
        };
        return exec_call(tree, cur_frame, left_obj, right_obj, is_debug);
    /* case OP_POS: */
        /* OP_POS would be discarded in tree parser */
    case OP_NEG:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        tmp_obj = copy_object(left_obj);
        tmp_obj.data.number.numer.sign = !tmp_obj.data.number.numer.sign;
        return OBJ_OBJERR(tmp_obj);
    case OP_NOT:
        if (is_bad_type(op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {
                .number = to_bool(left_obj) ? ONE_NUMBER() : ZERO_NUMBER()
            }
        }));
    case OP_GETL:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(copy_object(left_obj->data.pair.left));
    case OP_GETR:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(copy_object(left_obj->data.pair.right));
    case OP_EXP:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
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
            .type = TYPE_NUM,
            .data = {.number = number_exp(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_MUL:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_mul(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_DIV:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        if (right_obj->data.number.numer.size == 0) {
            print_runtime_error(
                op_token.pos.line,
                op_token.pos.col,
                "Divided by zero"
            );
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_div(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_MOD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_mod(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_ADD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_add(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_SUB:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_sub(
                &left_obj->data.number,
                &right_obj->data.number
            )}
        }));
    case OP_LT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                number_lt(&left_obj->data.number, &right_obj->data.number)
            )}
        }));
    case OP_LE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                !number_lt(&right_obj->data.number, &left_obj->data.number)
            )}
        }));
    case OP_GT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                number_lt(&right_obj->data.number, &left_obj->data.number)
            )}
        }));
    case OP_GE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                !number_lt(&left_obj->data.number, &right_obj->data.number)
            )}
        }));
    case OP_EQ:
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(object_eq(left_obj, right_obj))}
        }));
    case OP_NE:
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(!object_eq(left_obj, right_obj))}
        }));
    case OP_AND:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                to_bool(left_obj) && to_bool(right_obj)
            )}
        }));
    case OP_OR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return OBJ_OBJERR(((object_t) {
            .type = TYPE_NUM,
            .data = {.number = number_from_i32(
                to_bool(left_obj) || to_bool(right_obj)
            )}
        }));
    case OP_PAIR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        tmp_obj = (object_t) {
            .type = TYPE_PAIR,
            .data = {.pair = {
                .left = malloc(sizeof(object_t)),
                .right = malloc(sizeof(object_t)),
            }}
        };
        *tmp_obj.data.pair.left = copy_object(left_obj);
        *tmp_obj.data.pair.right = copy_object(right_obj);
        return OBJ_OBJERR(tmp_obj);
    case OP_FCALLR:
        if (is_bad_type(op_token, TYPE_FUNC, TYPE_ANY, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        return exec_call(tree, cur_frame, left_obj, right_obj, is_debug);
    case OP_CONDFCALL:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_PAIR, left_obj, right_obj)) {
            return ERR_OBJERR();
        }
        /* check inside the pair */
        {
            token_t tmp_op_token = (token_t) {
                .name = OP_CONDFCALL,
                .pos = op_token.pos,
                .type = TOK_OP,
                .str = "the members in the conditional function caller"
            };
            if (is_bad_type(
                tmp_op_token, TYPE_FUNC, TYPE_FUNC,
                right_obj->data.pair.left, right_obj->data.pair.right
            )) {
                return ERR_OBJERR();
            }

        }
        return exec_call(
            tree,
            cur_frame,
            (to_bool(left_obj)
                ? right_obj->data.pair.left : right_obj->data.pair.right),
            (object_t*) &RESERVED_OBJS[RESERVED_ID_NAME_NULL],
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
    const tree_t* tree,
    frame_t* cur_frame,
    const int entry_index,
    const const int is_debug
) {
    const token_t* global_tokens = tree->tokens.data;
    
    /* init obj_table_offset as entry_index because in postfix,
       parent's index is greater children's */
    int obj_table_offset = entry_index;
    unsigned long tree_size = 0;
    tree_preorder_iterator_t tree_iter;
    dynarr_t token_index_stack; /* type: int */
    object_t* obj_table;
    unsigned char* is_from_frame; /* is the object shallow copied from frame */
    unsigned char* is_evaled;

    token_t cur_token;
    int cur_index, cur_local_index, left_index, right_index;
    object_t* left_obj;
    object_t* right_obj;

    int i, is_error = 0;
    object_or_error_t tmp_obj_err;
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("eval_tree\n");
        printf("entry_index=%d cur_frame=%p ", entry_index, cur_frame);
    }
#endif
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
    /* if (entry_index - obj_table_offset + 1 != tree_size) {
        printf(
            "\neval_tree: bad tree. %d != %d\n",
            entry_index - obj_table_offset + 1,
            tree_size
        );
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
        exit(OTHER_ERR_CODE);
    } */
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
    }
#endif
    /* obj_table store shallow copyed intermidiate results */
    is_evaled = calloc(tree_size, sizeof(unsigned char));
    is_from_frame = calloc(tree_size, sizeof(unsigned char));
    obj_table = calloc(tree_size, sizeof(object_t));
    #define _IS_EVALED(index) (is_evaled[index - obj_table_offset])
    #define _IS_FROM_FRAME(index) (is_from_frame[index - obj_table_offset])
    #define _OBJ_TABLE(index) (obj_table[index - obj_table_offset])

    /* begin evaluation */
    token_index_stack = new_dynarr(sizeof(int));
    append(&token_index_stack, &entry_index);
    while (token_index_stack.size > 0) {
        cur_index = *(int*) back(&token_index_stack);
        cur_token = global_tokens[cur_index];
        left_index = tree->lefts[cur_index];
        right_index = tree->rights[cur_index];
#ifdef ENABLE_DEBUG
        if(is_debug) {
            printf("> (node %d) ", cur_index);
            print_token(cur_token);
            printf(
                " (local=%d) left=%d right=%d\n",
                cur_index - obj_table_offset, left_index, right_index
            );
            fflush(stdout);
        }
#endif
        left_obj = (left_index == -1 || !_IS_EVALED(left_index))
            ? NULL : &_OBJ_TABLE(left_index);
        right_obj = (right_index == -1 || !_IS_EVALED(right_index))
            ? NULL : &_OBJ_TABLE(right_index);

        if (cur_token.type == TOK_OP) {
            const token_t* left_token =
                (left_index == -1) ? NULL : &global_tokens[left_index];
            const token_t* right_token =
                (right_index == -1) ? NULL : &global_tokens[right_index];
            /* function definer */
            if (cur_token.name == OP_FMAKE) {
                _OBJ_TABLE(cur_index).type = TYPE_FUNC;
                (_OBJ_TABLE(cur_index)).data.func = (func_t) {
                    .is_pure = 0,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_name = -1,
                    .entry_index = left_index,
                    /* owns a deep copy of the frame it created under */
                    .init_time_frame = copy_frame(cur_frame)
                };
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(" defined function:");
                    print_object(&_OBJ_TABLE(cur_index), '\n');
                }
#endif
            }
            else if (cur_token.name == OP_LFMAKE) {
                _OBJ_TABLE(cur_index).type = TYPE_FUNC;
                (_OBJ_TABLE(cur_index)).data.func = (func_t) {
                    .is_pure = 1,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_name = -1,
                    .entry_index = left_index,
                    .init_time_frame = NULL
                };
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(" defined pure function:");
                    print_object(&_OBJ_TABLE(cur_index), '\n');
                }
#endif
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
                _OBJ_TABLE(cur_index) = *right_obj;
                *right_obj = NULL_OBJECT;
                _IS_FROM_FRAME(cur_index) = _IS_FROM_FRAME(right_index);
                _OBJ_TABLE(cur_index).data.func.arg_name = left_token->name;
            }
            /* assignment */
            else if (cur_token.name == OP_ASSIGN) {
                if (right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    continue;
                }
                if (frame_get(cur_frame, left_token->name) != NULL) {
                    sprintf(
                        ERR_MSG_BUF,
                        "Repeated initialization of identifier '%s'",
                        left_token->str
                    );
                    print_runtime_error(
                        left_token->pos.line,
                        left_token->pos.col,
                        ERR_MSG_BUF
                    );
                    is_error = 1;
                    break;
                }

                /* set frame */
                object_t* obj_on_frame =
                    frame_set(cur_frame, left_token->name, right_obj);
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("initialized identifier '%s' to ", left_token->str);
                    print_object(obj_on_frame, '\n');
                }
#endif
                _OBJ_TABLE(cur_index) = *obj_on_frame;
                _IS_FROM_FRAME(cur_index) = 1;
            }
            /* logic-and or logic-or  */
            else if (
                cur_token.name == OP_CONDAND || cur_token.name == OP_CONDOR
            ) {
                int is_condor = cur_token.name == OP_CONDOR;
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
                if (to_bool(left_obj) != is_condor) {
                    if (right_obj == NULL) {
                        append(&token_index_stack, &right_index);
                        continue;
                    }
                    /* move right to current */
                    _OBJ_TABLE(cur_index) = *right_obj;
                    *right_obj = NULL_OBJECT;
                    _IS_FROM_FRAME(cur_index) = _IS_FROM_FRAME(right_index);
                }
                else {
                    /* move left to current */
                    _OBJ_TABLE(cur_index) = *left_obj;
                    *left_obj = NULL_OBJECT;
                    _IS_FROM_FRAME(cur_index) = _IS_FROM_FRAME(left_index);
                }
            }
            else if (cur_token.name == OP_EXPRSEP) {
                if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* move right to current */
                _OBJ_TABLE(cur_index) = *right_obj;
                *right_obj = NULL_OBJECT;
                _IS_FROM_FRAME(cur_index) = _IS_FROM_FRAME(right_index);
            }
            /* other operator */
            else {
                if (IS_UNARY_OP[cur_token.name] && left_obj == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                else if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                tmp_obj_err = exec_op(
                    tree, cur_frame, cur_token, left_obj, right_obj, is_debug
                );
                if (tmp_obj_err.is_error) {
                    sprintf(
                        ERR_MSG_BUF,
                        "operator \"%s\" returns with error",
                        OP_STRS[cur_token.name]
                    );
                    print_runtime_error(
                        cur_token.pos.line,
                        cur_token.pos.col,
                        ERR_MSG_BUF
                    );
                }
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(
                        "^ (node %d) exec_op ",
                        cur_index, OP_STRS[cur_token.name]
                    );
                    print_token(cur_token);
                    printf(" safely returned\n");
                }
#endif
                /* end eval if error */
                if (tmp_obj_err.is_error) {
                    is_error = 1;
                    break;
                }
                _OBJ_TABLE(cur_index) = tmp_obj_err.obj;
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
            /* copy object from frame */
            _OBJ_TABLE(cur_index) = *o;
            _IS_FROM_FRAME(cur_index) = 1;
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf(
                    "  get identifiter '%s' (name=%d) from frame (addr=%p): ",
                    cur_token.str, cur_token.name, o
                );
                print_object(o, '\n');
            }
#endif
        }
        else if (cur_token.type == TOK_NUM) {
            _OBJ_TABLE(cur_index).type = TYPE_NUM;
            _OBJ_TABLE(cur_index).data.number = number_from_str(cur_token.str);
        }
        else {
            printf("eval_tree: bad token type: %d\n", cur_token.type);
            exit(RUNTIME_ERR_CODE);
        }
        _IS_EVALED(cur_index) = 1;
#ifdef ENABLE_DEBUG
        if (is_debug) {
            printf("< (node %d) eval result: ", cur_index);
            print_object(&_OBJ_TABLE(cur_index), '\n');
            fflush(stdout);
        }
#endif
        pop(&token_index_stack);
    }

    /* prepare return object */
    if (is_error) {
#ifdef ENABLE_DEBUG
        if (is_debug) {
            printf("eval_tree return with error\n");
        }
#endif
        tmp_obj_err = ERR_OBJERR();
    }
    else {
        tmp_obj_err = OBJ_OBJERR(copy_object(&_OBJ_TABLE(entry_index)));
    }

    /* free things */
    free_dynarr(&token_index_stack);
    free_tree_iter(&tree_iter);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("free obj_tables\n");
        fflush(stdout);
    }
#endif
    for (i = 0; i < tree_size; i++) {
        if (!is_from_frame[i]
            && obj_table[i].type != TYPE_NULL
            && obj_table[i].type != TYPE_BUILTIN_FUNC
        ) {
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf("free [%d]:", i); print_object(&obj_table[i], '\n');
                fflush(stdout);
            }
#endif
            free_object(&obj_table[i]);
        }
    }
    free(obj_table);
    free(is_from_frame);
    free(is_evaled);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("eval_tree returned ");
        print_object(&tmp_obj_err.obj, '\n');
        fflush(stdout);
    }
#endif
    return tmp_obj_err;
}
