from argparse import ArgumentParser
from lreng_lexer import tokenizer
from lreng_parser import tree_parser
from lreng_checker import check_tree_semantic
from lreng_evaler import eval_node, AnyType

def interpret_code(raw_str: str, is_debug: bool = False) -> AnyType:
    tokens = tokenizer(raw_str, is_debug=is_debug)
    root_node = tree_parser(tokens, is_debug=is_debug)
    if not check_tree_semantic(root_node, is_debug=is_debug):
        exit(3)
    eval_result = eval_node(root_node, is_debug=is_debug)
    if is_debug:
        print('eval_result:', eval_result)
    return eval_result

if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('input_file_path', nargs='?', const='', default='')
    parser.add_argument('--code', '-c', dest='raw_str', default='')
    parser.add_argument('--debug', action='store_true')
    args = parser.parse_args()
    if args.input_file_path != '':
        with open(args.input_file_path, 'r', encoding='utf8') as f:
            raw_str = f.read()
    else:
        raw_str = args.raw_str
    if raw_str == '':
        raise ValueError('code is empty')
    else:
        interpret_code(raw_str, args.debug)
