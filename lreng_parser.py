from lreng_objs import Token, TreeNode
from lreng_opers import (
    OP_PRECEDENCES,
    R_ASSO_OPS, UNARY_OPS, L_BRACKETS, R_BRACKETS,
    FUNC_CALL_L_PARENTHESE, FUNC_CALLER, FUNC_MAKER
)
from lreng_errs import throw_syntax_err_msg


PREFIXABLE_OPS = UNARY_OPS | L_BRACKETS - {FUNC_CALL_L_PARENTHESE}

def shunting_yard(tokens: list[Token], is_debug: bool = False) -> list[Token]:
    """Parse token sequence into postfix notation"""

    expection = 'prefixable'
    output = []
    op_stack = []

    def get_top():
        return op_stack[-1] if len(op_stack) > 0 else None

    for token in tokens:
        if is_debug:
            print(repr(token))
        if token.type == 'op':

            if token.raw in PREFIXABLE_OPS:
                if expection == 'binary_operator':
                    throw_syntax_err_msg(
                        token.db_pos_info,
                        f'Expect a binary operator, semicolon, '
                        f'or closing bracket. Get {repr(token.raw)}.'
                    )
            else:
                if expection == 'prefixable':
                    throw_syntax_err_msg(
                        token.db_pos_info,
                        f'Expect a unary_operator, number, identifier, '
                        f'or open bracket. Get {repr(token.raw)}.'
                    )
                expection = 'prefixable'

            top = get_top()

            def is_top_lower(top, t):
                t_precedence = OP_PRECEDENCES[t.raw]
                is_t_left_asso = t.raw not in R_ASSO_OPS
                if top is not None:
                    top_precedences = OP_PRECEDENCES[top.raw]
                    return (
                        top_precedences < t_precedence
                        or (top_precedences == t_precedence and is_t_left_asso)
                    )
                return False

            while is_top_lower(top, token) and top.raw not in L_BRACKETS:
                # print('top', top)
                output.append(top)
                op_stack.pop()
                top = get_top()
            if token.raw in R_BRACKETS:
                expection = 'binary_operator'
                # pop until left bracket is meet
                while top.raw not in L_BRACKETS:
                    output.append(top)
                    op_stack.pop()
                    top = get_top()
                    if top is None:
                        throw_syntax_err_msg(
                            token.db_pos_info,
                            'Unpaired closing bracket.'
                        )
                # pop out the left bracket
                l_bracket = op_stack.pop()
                top = get_top()
                # if the l_brackets is part of function call
                if (l_bracket.raw == FUNC_CALL_L_PARENTHESE
                        and token.raw == ')'):
                    # replace it to function caller
                    output.append(Token(FUNC_CALLER, 'op', token.db_pos_info))

                # if the l_bracket is part of function maker
                if l_bracket.raw == '{' and token.raw == '}':
                    # add function code indicator
                    output.append(Token(FUNC_MAKER, 'op'))
            else:
                op_stack.append(token)

        else:
            if expection == 'binary_operator':
                throw_syntax_err_msg(
                    token.db_pos_info,
                    f'Expect a binary operator, semicolon, '
                    f'or closing bracket. Get {repr(token.raw)}.'
                )
            expection = 'binary_operator'
            output.append(token)

        if is_debug:
            print('stack:\t', op_stack)
            print('out:\t', output)
    # end for
    # empty the stack
    output += op_stack[::-1]
    if is_debug:
        print('final:\t', output)
    return output


def postfix_to_tree(
        postfix_token_list: list[Token],
        is_debug: bool = False) -> TreeNode:
    stack = list()
    for token in postfix_token_list:
        if token.type == 'op':
            r_node = stack.pop() if token.raw not in UNARY_OPS else None
            if len(stack) == 0:
                throw_syntax_err_msg(
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
    assert len(stack) == 1, \
        f'Bad syntax somewhere: nodes remained in stack {stack}'
    return stack[0]

def tree_parser(tokens: list[Token], is_debug=False) -> TreeNode:
    postfix_token_list = shunting_yard(tokens, is_debug)
    syntax_tree = postfix_to_tree(postfix_token_list, is_debug)
    return syntax_tree


if __name__ == '__main__':
    from lreng_lexer import tokenizer
    print(tree_parser(tokenizer('+3 + 4 * -2 / (1-5) ^ 2 ^ 3'), True))
    print(tree_parser(tokenizer('sin( max (2, 3) / 3 * pi)')), True)
    print(tree_parser(tokenizer('add = p : { `p + ~p }')), True)
