from string import ascii_letters, digits, whitespace
from lreng_opers import ALL_OPS

NUM_BIN_PREFIX = '0b'
NUM_HEX_PREFIX = '0x'
HEX_CHARS = set(digits + 'ABCDEFabcdef')
CHAR_ESC_TABLE = {
    'a': '\a',
    'b': '\b',
    'n': '\n',
    'r': '\r',
    't': '\t',
    'v': '\v',
    '\\': '\\',
    '\'': '\''
}

COMMENT_STARTER = '#'
WS_CHARS = set(whitespace)
ID_CHARS = set(ascii_letters + digits + '_')
NUM_CHARS = set(digits + '.')
OP_CHARS = set(r''.join(ops for ops in ALL_OPS))
