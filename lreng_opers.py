
operator_hierarchies = [
    # function code maker
    ('{', '}'),
    # parenthesis / function call, right-associative function caller
    ('(', ')', '$'),
    # unary plus and minus, logic not, get left of pair, get right of pair
    ('!+', '!-', '!', '`', '~'),
    # power
    ('^',),
    # multiplication, division, remainder
    ('*', '/', '%'),
    # addition, subtraction
    ('+', '-'),
    # write a byte to stdout and return null
    ('<<', ),
    # inequality comparisons
    ('<', '<=', '>', '>='),
    # equal, not equal
    ('==', '!='),
    # logic and
    ('&',),
    # logic or
    ('|',),
    # pair maker
    (',',),
    # function argument adder
    # arg_id : { function codes }
    (':',),
    # if operation
    # `a ? b` evaluates `b` when `a` evaluates to true, otherwise null
    ('?',),
    # assignment
    ('=',),
    # expression connector
    (';',)
]

all_ops = {
    op
    for ops in operator_hierarchies
    for op in ops
}

op_precedences = {
    op: p
    for p, ops in enumerate(operator_hierarchies)
    for op in ops
}

# auxiliary operators
FUNC_CALL_L_PARENTHESE = '['
FUNC_MAKER = '@'
# it means the parenthesis is part of a function call
# and will be replaced to `$`
op_precedences['['] = op_precedences['(']
# function code block indicator '@' is unary and make the code nodes underneath
# evaluated as a code block
op_precedences['@'] = op_precedences['{']

FUNC_CALLER = '$'
ARG_SETTER = ':'
ASSIGNMENT = '='
IF_OP = '?'
EXPR_CONNECTOR = ';'

R_ASSO_OPS = {'!+', '!-', '!', '`', '~', '$', '^', ',', ':', '='}
UNARY_OPS = {'@', '!+', '!-', '!', '`', '~', '<<'}
L_BRACKETS = {'(', '{', FUNC_CALL_L_PARENTHESE}
R_BRACKETS = {')', '}'}
