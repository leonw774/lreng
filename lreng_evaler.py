import sys
from typing import Callable
from copy import copy

from lreng_opers import (
    UNARY_OPS, FUNC_MAKER, ARG_SETTER, ASSIGNMENT, IF_AND, IF_OR
)
from lreng_objs import (
    Frame, Token, TreeNode,
    GeneralObj, FuncObj, NumObj, PairObj, NullObj
)

def do_call(func_obj: FuncObj, arg_obj: GeneralObj) -> GeneralObj:
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

def do_write_byte(num_obj: NumObj) -> NullObj:
    assert num_obj.value.denominator == 1, num_obj.value
    v = int(num_obj.value)
    assert 0 <= v <= 255
    sys.stdout.buffer.write(bytes((v,)))
    sys.stdout.flush()
    return NullObj()

op_func_configs = {
    '$': ((FuncObj, GeneralObj), do_call),
    '!+': ((NumObj,), lambda x: NumObj(x.value)),
    '!-': ((NumObj,), lambda x: NumObj(-x.value)),
    '!': ((NumObj,), lambda x: NumObj(x.value == 0)),
    '`': ((PairObj,), lambda x: x.left),
    '~': ((PairObj,), lambda x: x.right),
    '^': ((NumObj, NumObj), lambda x, y: NumObj(x.value ** y.value)),
    '*': ((NumObj, NumObj), lambda x, y: NumObj(x.value * y.value)),
    '/': ((NumObj, NumObj), lambda x, y: NumObj(x.value / y.value)),
    '%': ((NumObj, NumObj), lambda x, y: NumObj(x.value % y.value)),
    '+': ((NumObj, NumObj), lambda x, y: NumObj(x.value + y.value)),
    '-': ((NumObj, NumObj), lambda x, y: NumObj(x.value - y.value)),
    '<<': ((NumObj,), do_write_byte),
    # '>>': ((NumObj,), lambda x: NotImplementedError()),
    '<': ((NumObj, NumObj), lambda x, y: NumObj(x.value < y.value)),
    '>': ((NumObj, NumObj), lambda x, y: NumObj(x.value > y.value)),
    '<=': ((NumObj, NumObj), lambda x, y: NumObj(x.value <= y.value)),
    '>=': ((NumObj, NumObj), lambda x, y: NumObj(x.value >= y.value)),
    '==': ((GeneralObj, GeneralObj), lambda x, y: NumObj(x == y)),
    '!=': ((GeneralObj, GeneralObj), lambda x, y: NumObj(x != y)),
    '&': ((GeneralObj, GeneralObj), lambda x, y: NumObj(bool(x) and bool(y))),
    '|': ((GeneralObj, GeneralObj), lambda x, y: NumObj(bool(x) or bool(y))),
    ',': ((GeneralObj, GeneralObj), PairObj),
    ';': ((GeneralObj, GeneralObj), lambda x, y: y),
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

op_funcs = {
    op: op_func_builder(*op_func_config)
    for op, op_func_config in op_func_configs.items()
}


def throw_runtime_err_msg(pos: tuple[int, int], msg: str):
    print(f'[RumtimeError]: Line {pos[0]} col {pos[1]}: {msg}')
    exit(2)

G_IS_DEBUG = False
def eval_postfix(postfix_token_list: list[Token], is_debug=False) -> GeneralObj:
    tree_root = postfix_to_tree(postfix_token_list, is_debug=is_debug)
    if is_debug:
        print(TreeNode.dump(tree_root))
    global G_IS_DEBUG
    G_IS_DEBUG = is_debug
    return eval_node(tree_root)

def postfix_to_tree(postfix_token_list: list[Token], is_debug=False) -> TreeNode:
    stack = list()
    for token in postfix_token_list:
        if token.type == 'op':
            r_node = stack.pop() if token.raw not in UNARY_OPS else None
            if len(stack) == 0:
                throw_runtime_err_msg(
                    token.db_pos_info,
                    f'operator {repr(token)} has too few operands'
                )
            l_node = stack.pop()
            if is_debug:
                print(token, f'L={l_node}, R={r_node}')
            stack.append(TreeNode(tok=token, left=l_node, right=r_node))
        else:
            if is_debug:
                print(token)
            stack.append(TreeNode(tok=token))
    if is_debug:
        print(stack)
    assert len(stack) == 1, \
        f'Bad syntax somewhere: nodes remained in stack {stack}'
    return stack[0]


class NodeEvalDict:
    def __init__(self) -> None:
        self.table = dict()

    def __getitem__(self, node: TreeNode) -> GeneralObj | None:
        return self.table.get(id(node), None)

    def __setitem__(self, node: TreeNode, value: GeneralObj) -> None:
        self.table[id(node)] = value

def eval_node(
        root_node: TreeNode,
        inherent_id_obj_table: Frame | None = None) -> GeneralObj:
    if inherent_id_obj_table is None:
        id_obj_table = Frame(local={'null': NullObj()})
    else:
        assert isinstance(inherent_id_obj_table, Frame)
        id_obj_table = inherent_id_obj_table
    if G_IS_DEBUG:
        print('Step into node', root_node)
        print('initial id_obj_table:', id_obj_table)

    node_eval_to = NodeEvalDict()

    node_stack: list[TreeNode] = [root_node]
    while len(node_stack) > 0:
        if G_IS_DEBUG:
            print('stack', node_stack)
        node = node_stack[-1]
        if node.tok.type == 'op':
            if node.tok.raw in UNARY_OPS:
                if node.tok.raw == FUNC_MAKER:
                    node_eval_to[node] = FuncObj(
                        code_root_node = node.left,
                        id_obj_table=id_obj_table,
                        arg_id=None
                    )
                    if G_IS_DEBUG:
                        print('op', node, 'eval to:', node_eval_to[node])
                elif node_eval_to[node.left] is None:
                    node_stack.append(node.left)
                    continue
                else:
                    node_op_func = op_funcs[node.tok.raw]
                    args = (node_eval_to[node.left], )
                    if G_IS_DEBUG:
                        print('op:', node, 'args:', args)
                    try:
                        node_eval_to[node] = node_op_func(args)
                    except AssertionError as ae:
                        throw_runtime_err_msg(
                            node.tok.db_pos_info,
                            f'Operator {repr(node.tok.raw)} says "{str(ae)}"'
                        )
                    if G_IS_DEBUG:
                        print('eval to:', node_eval_to[node])
            else:
                if node.tok.raw == ARG_SETTER:
                    if node.left.tok.type != 'id':
                        throw_runtime_err_msg(
                            'Left side of argument setter should be identifier'
                        )
                    if node_eval_to[node.right] is None:
                        node_stack.append(node.right)
                        continue
                    else:
                        if not isinstance(node_eval_to[node.right], FuncObj):
                            throw_runtime_err_msg(
                                node.right.tok.db_pos_info,
                                'Right side of argument setter should be '
                                'function block'
                            )
                        node_eval_to[node] = copy(node_eval_to[node.right])
                        node_eval_to[node].arg_id = node.left.tok.raw

                elif node.tok.raw == ASSIGNMENT:
                    if node.left is None or node.left.tok.type != 'id':
                        throw_runtime_err_msg(
                            node.left.tok.db_pos_info,
                            f'Line {node.left.tok.db_pos_info[0]} '
                            f'col {node.left.tok.db_pos_info[1]}: '
                            f'Left side of assignment should be identifier. '
                            f'Get {node.left}'
                        )
                    if node.left.tok.raw in id_obj_table:
                        throw_runtime_err_msg(
                            node.left.tok.db_pos_info,
                            f'Line {node.left.tok.db_pos_info[0]} '
                            f'col {node.left.tok.db_pos_info[1]}: '
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
                        if G_IS_DEBUG:
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
                        if G_IS_DEBUG:
                            print('op', node, 'eval to:', node_eval_to[node])

                elif (node_eval_to[node.left] is None
                        and node_eval_to[node.right] is None):
                    node_stack.append(node.right)
                    node_stack.append(node.left)
                    continue

                else:
                    node_op_func = op_funcs[node.tok.raw]
                    args = (node_eval_to[node.left], node_eval_to[node.right])
                    if G_IS_DEBUG:
                        print('op:', node, 'args:', args)
                    try:
                        node_eval_to[node] = node_op_func(args)
                    except AssertionError as ae:
                        throw_runtime_err_msg(
                            node.tok.db_pos_info,
                            f'Operator {repr(node.tok.raw)} says "{str(ae)}"'
                        )
                    if G_IS_DEBUG:
                        print('eval to:', node_eval_to[node])

        elif node.tok.type == 'id':
            obj = id_obj_table.get(node.tok.raw, None)
            if obj is None:
                throw_runtime_err_msg(
                    f'Line {node.tok.db_pos_info[0]} '
                    f'col {node.tok.db_pos_info[1]}: '
                    f'identifier {repr(node.tok.raw)} is used uninitialized')
            if G_IS_DEBUG:
                print('update id-obj table:', id_obj_table)
            node_eval_to[node] = obj

        elif node.tok.type == 'num':
            node_eval_to[node] = NumObj(node.tok.raw)

        else:
            raise ValueError(f'Bad node type: {node.tok.type}')
        node_stack.pop()
    # end while
    return node_eval_to[root_node]
