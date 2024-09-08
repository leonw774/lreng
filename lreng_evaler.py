from collections.abc import Callable
from copy import copy

from lreng_opers import (
    UNARY_OPS, FUNC_CALLER, FUNC_MAKER, ARG_SETTER, ASSIGNMENT, IF_AND, IF_OR
)
from lreng_chars import (
    CHAR_ESC_TABLE, NUM_BIN_PREFIX, NUM_HEX_PREFIX
)
from lreng_objs import (
    Frame, TreeNode, AnyType, Function, Number, Pair, Null
)
from lreng_builtins import DEFAULT_FRAME, BUILTIN_FUNC_DOERS
from lreng_errs import throw_runtime_err_msg


## operator functions

def do_call(func_obj: Function, arg_obj: AnyType) -> AnyType:
    # if built-in
    if func_obj.code_root_node.tok.raw in BUILTIN_FUNC_DOERS:
        try:
            func_name = func_obj.code_root_node.tok.raw
            eval_result = BUILTIN_FUNC_DOERS[func_name](arg_obj)
        except AssertionError as ae:
            raise AssertionError(
                f'In built-in function {func_name}: {str(ae)}'
            ) from ae
        return eval_result
    # otherwise
    # copy frame
    currernt_frame = Frame(
        local={func_obj.arg_id: arg_obj},
        source=func_obj.id_obj_table
    )
    # call function
    return eval_node(
        func_obj.code_root_node,
        inherent_id_obj_table=currernt_frame
    )

def do_cond_func_pair_call(
        cond_obj: AnyType,
        func_pair_obj: Pair) -> AnyType:
    assert isinstance(func_pair_obj.left, Function), \
        (
            'The left element is not a function '
            f'but a {type(func_pair_obj.left).__name__}.'
        )
    assert isinstance(func_pair_obj.right, Function), \
        (
            'The right element is not a function '
            f'but a {type(func_pair_obj.left).__name__}.'
        )
    return (
        do_call(func_pair_obj.left, Null())
        if bool(cond_obj) else
        do_call(func_pair_obj.right, Null())
    )

op_func_configs = {
    '$': ((Function, AnyType), do_call),
    '!+': ((Number,), lambda x: Number(x.value)),
    '!-': ((Number,), lambda x: Number(-x.value)),
    '!': ((Number,), lambda x: Number(x.value == 0)),
    '`': ((Pair,), lambda x: x.left),
    '~': ((Pair,), lambda x: x.right),
    '^': ((Number, Number), lambda x, y: Number(x.value ** y.value)),
    '*': ((Number, Number), lambda x, y: Number(x.value * y.value)),
    '/': ((Number, Number), lambda x, y: Number(x.value / y.value)),
    '%': ((Number, Number), lambda x, y: Number(x.value % y.value)),
    '+': ((Number, Number), lambda x, y: Number(x.value + y.value)),
    '-': ((Number, Number), lambda x, y: Number(x.value - y.value)),
    '<': ((Number, Number), lambda x, y: Number(x.value < y.value)),
    '>': ((Number, Number), lambda x, y: Number(x.value > y.value)),
    '<=': ((Number, Number), lambda x, y: Number(x.value <= y.value)),
    '>=': ((Number, Number), lambda x, y: Number(x.value >= y.value)),
    '==': ((AnyType, AnyType), lambda x, y: Number(x == y)),
    '!=': ((AnyType, AnyType), lambda x, y: Number(x != y)),
    '&': ((AnyType, AnyType), lambda x, y: Number(bool(x) and bool(y))),
    '|': ((AnyType, AnyType), lambda x, y: Number(bool(x) or bool(y))),
    ',': ((AnyType, AnyType), Pair),
    '?': ((AnyType, Pair), do_cond_func_pair_call),
    ';': ((AnyType, AnyType), lambda x, y: y),
}

def op_func_builder(arg_types: tuple, real_func: Callable):
    def f(args):
        assert len(args) == len(arg_types), \
            f'Bad argument number: want {len(arg_types)} but get {len(args)}'
        for arg, arg_type in zip(args, arg_types):
            assert isinstance(arg, arg_type), \
                f'Argument {arg} is not of type {arg_type.__name__}'
        return real_func(*args)
    return f

OP_FUNC_DOERS = {
    op: op_func_builder(*op_func_config)
    for op, op_func_config in op_func_configs.items()
}


class NodeEvalDict:
    def __init__(self) -> None:
        self.table = dict()

    def __getitem__(self, node: TreeNode) -> AnyType | None:
        return self.table.get(id(node), None)

    def __setitem__(self, node: TreeNode, value: AnyType) -> None:
        self.table[id(node)] = value



def eval_node(
        root_node: TreeNode,
        inherent_id_obj_table: Frame | None = None,
        is_debug: bool = False) -> AnyType:
    if inherent_id_obj_table is None:
        id_obj_table = Frame(local=DEFAULT_FRAME)
    else:
        assert isinstance(inherent_id_obj_table, Frame)
        id_obj_table = inherent_id_obj_table
    if is_debug:
        print('Step into node', root_node)
        print('initial id_obj_table:', id_obj_table)

    node_eval_to = NodeEvalDict()

    node_stack: list[TreeNode] = [root_node]
    while len(node_stack) > 0:
        if is_debug:
            print('stack', node_stack)
        node = node_stack[-1]
        if node.tok.type == 'op':
            # special ops
            if node.tok.raw == FUNC_MAKER:
                node_eval_to[node] = Function(
                    code_root_node = node.left,
                    id_obj_table=id_obj_table,
                    arg_id=None
                )
                if is_debug:
                    print('op', node, 'eval to:', node_eval_to[node])

            elif node.tok.raw == ARG_SETTER:
                if node.left.tok.type != 'id':
                    throw_runtime_err_msg(
                        'Left side of argument setter should be identifier'
                    )
                if node_eval_to[node.right] is None:
                    node_stack.append(node.right)
                    continue
                else:
                    if not isinstance(node_eval_to[node.right], Function):
                        throw_runtime_err_msg(
                            node.right.tok.db_pos_info,
                            'Right side of argument setter should be '
                            'function block'
                        )
                    node_eval_to[node] = copy(node_eval_to[node.right])
                    node_eval_to[node].arg_id = node.left.tok.raw

            elif node.tok.raw == ASSIGNMENT:
                if node.left.tok.raw in id_obj_table:
                    throw_runtime_err_msg(
                        node.left.tok.db_pos_info,
                        f'Identifier {repr(node.left.tok.raw)} '
                        'is already initialized.'
                    )
                if node_eval_to[node.right] is None:
                    node_stack.append(node.right)
                    continue
                else:
                    id_str = node.left.tok.raw
                    id_obj_table[id_str] = node_eval_to[node.right]
                    node_eval_to[node] = node_eval_to[node.right]
                    if is_debug:
                        print('update id-obj table:', id_obj_table)

            elif node.tok.raw == IF_AND or node.tok.raw == IF_OR:
                # eval left first
                if node_eval_to[node.left] is None:
                    node_stack.append(node.left)
                    continue
                else:
                    # truth table
                    #  left | is_if_or | is_right_evaled | eval_result
                    # ------|----------|-----------------|-------------
                    # false | false    | false           | left
                    # false | true     | true            | right
                    # true  | false    | true            | right
                    # true  | true     | false           | left
                    is_left_true = bool(node_eval_to[node.left])
                    is_if_or = node.tok.raw == IF_OR
                    if is_left_true != is_if_or:
                        if node_eval_to[node.right] is None:
                            node_stack.append(node.right)
                            continue
                        else:
                            node_eval_to[node] = node_eval_to[node.right]
                    else:
                        node_eval_to[node] = node_eval_to[node.left]
                    if is_debug:
                        print('op', node, 'eval to:', node_eval_to[node])
            # unary
            elif node.tok.raw in UNARY_OPS:
                if node_eval_to[node.left] is None:
                    node_stack.append(node.left)
                    continue
                else:
                    node_op_func = OP_FUNC_DOERS[node.tok.raw]
                    args = (node_eval_to[node.left], )
                    if is_debug:
                        print('op:', node, 'args:', args)
                    try:
                        node_eval_to[node] = node_op_func(args)
                    except AssertionError as ae:
                        throw_runtime_err_msg(
                            node.tok.db_pos_info,
                            f'Operator {repr(node.tok.raw)} says "{str(ae)}"'
                        )
                    if is_debug:
                        print('eval to:', node_eval_to[node])
            # binary
            else:
                if (node_eval_to[node.left] is None
                        and node_eval_to[node.right] is None):
                    node_stack.append(node.right)
                    node_stack.append(node.left)
                    continue
                else:
                    node_op_func = OP_FUNC_DOERS[node.tok.raw]
                    args = (node_eval_to[node.left], node_eval_to[node.right])
                    if is_debug:
                        print('op:', node, 'args:', args)
                    try:
                        node_eval_to[node] = node_op_func(args)
                    except AssertionError as ae:
                        if node.tok.raw == FUNC_CALLER:
                            throw_runtime_err_msg(
                                node.tok.db_pos_info,
                                f'Function {repr(node.left.tok.raw)} says '
                                f'{repr(str(ae))}'
                            )
                        throw_runtime_err_msg(
                            node.tok.db_pos_info,
                            f'Operator {repr(node.tok.raw)} says "{str(ae)}"'
                        )
                    except ZeroDivisionError:
                        throw_runtime_err_msg(
                            node.tok.db_pos_info,
                            'Divied by zero'
                        )
                    if is_debug:
                        print('eval to:', node_eval_to[node])

        elif node.tok.type == 'id':
            obj = id_obj_table.get(node.tok.raw, None)
            if obj is None:
                throw_runtime_err_msg(
                    node.tok.db_pos_info,
                    f'Identifier {repr(node.tok.raw)} is used uninitialized'
                )
            if is_debug:
                print('update id-obj table:', id_obj_table)
            node_eval_to[node] = obj

        elif node.tok.type == 'num':
            if node.tok.raw.startswith((NUM_HEX_PREFIX, NUM_BIN_PREFIX)):
                node_eval_to[node] = Number(int(node.tok.raw, base=0))
            else:
                node_eval_to[node] = Number(node.tok.raw)

        elif node.tok.type == 'chr':
            if len(node.tok.raw) == 1:
                node_eval_to[node] = Number(ord(node.tok.raw))
            else:
                c = CHAR_ESC_TABLE[node.tok.raw[1]]
                node_eval_to[node] = Number(ord(c))

        else:
            raise ValueError(f'Bad node type: {node.tok.type}')
        node_stack.pop()
    # end while
    return node_eval_to[root_node]
