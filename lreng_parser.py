from lreng_lexer import Token
from lreng_opers import (
    op_precedences,
    right_associative_ops,
    l_brackets,
    r_brackets,
    function_call_l_parenth,
    function_caller,
    function_maker
)

def shunting_yard(
        tokens: list[Token],
        is_debug=False
    ) -> list[Token]:
    """Parse token sequence into postfix notation"""

    output = []
    op_stack = []
    def get_top():
        return op_stack[-1] if len(op_stack) > 0 else None
    for t in tokens:
        if is_debug:
            print(repr(t))
        if t.type == 'op':
            t_precedence = op_precedences[t.raw]
            is_t_left_asso = t.raw not in right_associative_ops
            top = get_top()

            def is_top_lower(top, t):
                if top is not None:
                    top_precedences = op_precedences[top.raw]
                    return (
                        top_precedences < t_precedence
                        or (top_precedences == t_precedence and is_t_left_asso)
                    )
                return False

            while is_top_lower(top, t) and top.raw not in l_brackets:
                # print('top', top)
                output.append(top)
                op_stack.pop()
                top = get_top()
            if t.raw not in r_brackets:
                op_stack.append(t)
            else:
                # pop until left bracket is meet
                while top.raw not in l_brackets:
                    output.append(top)
                    op_stack.pop()
                    top = get_top()
                # pop out the left bracket
                l_bracket = op_stack.pop()
                top = get_top()
                # if the l_brackets is part of function call
                if l_bracket.raw == function_call_l_parenth and t.raw == ')':
                    # replace it to function caller
                    output.append(Token(raw=function_caller, tok_type='op'))

                # if the l_bracket is part of function maker
                if l_bracket.raw == '{' and t.raw == '}':
                    # add function code indicator 
                    output.append(Token(raw=function_maker, tok_type='op'))
        else:
            output.append(t)
        
        if is_debug:
            print('stack:\t', op_stack)
            print('out:\t', output)
    # end for
    # empty the stack
    output += op_stack[::-1]
    if is_debug:
        print('final:\t', output)
    return output

if __name__ == '__main__':
    print(shunting_yard(
        ['!+', '3', '+', '4', '*', '!-', '2', '/', '(', '1', '-', '5', ')', '^', '2', '^', '3', ';'],
    ))

    print(shunting_yard(
        ['sin', '(', 'max', '(', '2', ',', '3', ')', '/', '3', '*', 'pi', ')', ';'],
        {'sin', 'max'}
    ))

    print(shunting_yard(
        ['add', '=', 'p', ':', '{', '@', 'p', '+', '$', 'p', ';', '}']
    ))