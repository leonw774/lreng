
operator_hierarchies = [
    # function code maker
    ('{', '}'),
    # parenthesis / function call, function caller
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
# add temperary operator '$('
# it means the parenthesis is part of a function call
op_precedences['$('] = op_precedences['(']
# add function code indicator '@'
op_precedences['@'] = op_precedences['{']

R_ASSO_OPS = {'!+', '!-', '!', '^', '`', '~', ':', '=', ','}

UNARY_OPS = {'@', '!+', '!-', '!', '`', '~', '<<', '>>'}

L_BRACKETS = {'(', '{', '$('}
R_BRACKETS = {')', '}'}

FUNC_CALL_L_PARENTHESE = '$('
FUNC_MAKER = '@'
FUNC_CALLER = '$'
ARG_SETTER = ':'
ASSIGNMENT = '='
IF_OP = '?'
EXPR_CONNECTOR = ';'
