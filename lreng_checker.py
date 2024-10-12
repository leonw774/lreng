from lreng_opers import ASSIGNMENT, ARG_SETTER, FUNC_MAKER
from lreng_objs import TreeNode, Frame
from lreng_errs import print_semantic_err_msg
from lreng_builtins import DEFAULT_FRAME

# node rule checkers

def check_tree_semantic(root_node: TreeNode, is_debug: bool = False) -> bool:
    is_good_semantic = True
    all_ids = set()
    used_ids = dict()
    cur_frame = Frame(DEFAULT_FRAME)
    func_depth_stack = [-1]
    if is_debug:
        print('check_tree_semantic')
    for node, depth in root_node.iter_with_depth():
        print(func_depth_stack)
        print(cur_frame)
        # check left variable initialization
        if node.tok.type == 'op':
            if depth <= func_depth_stack[-1]:
                # we leave the function
                func_depth_stack.pop()
                cur_frame = cur_frame.shared
            if node.tok.raw == ASSIGNMENT or node.tok.raw == ARG_SETTER:
                cur_id = node.left.tok.raw
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
                        f'Get {repr(cur_id)}'
                    )
                elif cur_id in cur_frame:
                    is_good_semantic = False
                    print_semantic_err_msg(
                        node.left.tok.db_pos_info,
                        'Repeated initialization of identifier '
                        f'{repr(cur_id)}.'
                    )
                else:
                    cur_frame[cur_id] = True
                    all_ids.add(cur_id)

            if node.tok.raw == FUNC_MAKER:
                func_depth_stack.append(depth)
                cur_frame = Frame(dict(), cur_frame)

        elif node.tok.type == 'id':
            if node.tok.raw not in DEFAULT_FRAME:
                if node.tok.raw not in used_ids:
                    used_ids[node.tok.raw] = []
                used_ids[node.tok.raw].append(
                    (cur_frame, node.tok.db_pos_info)
                )

    uninited_ids = set()
    # for all used id, check if it is inited in its Frame
    for used_id, occurence_list in used_ids.items():
        uninited_ids.update([
            (used_id, db_pos_info)
            for frame, db_pos_info in occurence_list
            if used_id not in frame
        ])
    if is_debug:
        print('Initialized:', all_ids)
        print('Used', set(used_ids))
        print('Uninitialized used', uninited_ids)
    if len(uninited_ids) != 0:
        is_good_semantic = False
        for uninited_id, pos in uninited_ids:
            print_semantic_err_msg(
                pos,
                f'Identifier {repr(uninited_id)} is used but '
                'never initilized in this scope.'
            )

    return is_good_semantic
