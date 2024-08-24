from lreng_opers import ASSIGNMENT, ARG_SETTER
from lreng_objs import TreeNode
from lreng_errs import print_semantic_err_msg
from lreng_evaler import DEFAULT_FRAME

# node rule checkers

def check_tree_semantic(root_node: TreeNode, is_debug: bool = False) -> bool:
    is_good_semantic = True
    inited_ids = set()
    used_ids = dict()
    if is_debug:
        print('check_tree_semantic')
    for node in root_node:
        # check left variable initialization
        if node.tok.type == 'op':
            if node.tok.raw == ASSIGNMENT or node.tok.raw == ARG_SETTER:
                if node.left is None or node.left.tok.type != 'id':
                    is_good_semantic = False
                    op_name = (
                        'assignment'
                        if node.tok.raw == ASSIGNMENT else
                        'argument setter'
                    )
                    print_semantic_err_msg(
                        node.left.tok.db_pos_info,
                        f'Left side of {op_name} should be identifier. '
                        f'Get {node.left}'
                    )
                else:
                    inited_ids.add(node.left.tok.raw)

        elif node.tok.type == 'id':
            if node.tok.raw not in DEFAULT_FRAME:
                if node.tok.raw not in used_ids:
                    used_ids[node.tok.raw] = []
                used_ids[node.tok.raw].append(node.tok.db_pos_info)

    uninited_ids = set(used_ids) - inited_ids
    if is_debug:
        print('Initialized:', inited_ids)
        print('Used', used_ids)
        print('Uninitialized', uninited_ids)
    if len(uninited_ids) != 0:
        is_good_semantic = False
        for uninited_var in uninited_ids:
            for var_used_pos in used_ids[uninited_var]:
                print_semantic_err_msg(
                    var_used_pos,
                    f'Indetifier {uninited_var} is used but never initilized.'
                )

    return is_good_semantic
