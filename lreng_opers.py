
OPERATOR_HIERARCHIES = [
    # function code maker
    ('{', '}'),
    # parenthesis / function call
    ('(', ')'),
    # unary plus and minus, logic not, get left of pair, get right of pair
    ('!+', '!-', '!', '`', '~'),
    # power
    ('^',),
    # multiplication, division, remainder
    ('*', '/', '%'),
    # addition, subtraction
    ('+', '-'),
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
    # right-associative function caller
    ('$',),
    # function argument adder
    # arg_id => { function codes }
    ('=>',),
    # short-circuit logic and
    ('&&',),
    # short-circuit logic or
    ('||',),
    # assignment, conditional function pair caller
    ('=', '?'),
    # expression seperator
    (';',)
]

ALL_OPS = {
    op
    for ops in OPERATOR_HIERARCHIES
    for op in ops
}

OP_PRECEDENCES = {
    op: p
    for p, ops in enumerate(OPERATOR_HIERARCHIES)
    for op in ops
}

# auxiliary operators
FUNC_CALL_L_PARENTHESE = '['
FUNC_MAKER = '@'
# it means the parenthesis is part of a function call
# and will be replaced to `$`
OP_PRECEDENCES['['] = OP_PRECEDENCES['(']
# function code block indicator '@' is unary and make the code nodes underneath
# evaluated as a code block
OP_PRECEDENCES['@'] = OP_PRECEDENCES['{']

FUNC_CALLER = '$'
ARG_SETTER = '=>'
IF_AND = '&&'
IF_OR = '||'
ASSIGNMENT = '='

R_ASSO_OPS = {
    '!+', '!-', '!', '`', '~',
    '^', FUNC_CALLER, ',', ARG_SETTER, '?', '='
}
UNARY_OPS = {'@', '!+', '!-', '!', '`', '~'}
L_BRACKETS = {'(', '{', FUNC_CALL_L_PARENTHESE}
R_BRACKETS = {')', '}'}
