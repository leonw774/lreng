import sys

from lreng_objs import (
    TreeNode, Token,
    AnyType, Function, Number, Null
)

def do_output(num_obj: Number) -> Null:
    assert isinstance(num_obj, Number), f'{num_obj} is not a number'
    assert num_obj.value.denominator == 1, \
        f'{float(num_obj.value)} is not a valid ASCII code.'
    v = int(num_obj.value)
    assert 0 <= v <= 255, \
        f'{int(num_obj.value)} is not a valid ASCII code.'
    sys.stdout.buffer.write(bytes((v,)))
    sys.stdout.flush()
    return Null()

def do_input(_obj: AnyType) -> Number:
    read_byte = sys.stdin.buffer.read(1)
    return Number(int.from_bytes(read_byte, 'little'))

BUILTIN_FUNC_DOERS = {
    'output': do_output,
    'input': do_input
}

BUILTIN_VARS = {
    'null': Null()
}

DEFAULT_FRAME = BUILTIN_VARS
DEFAULT_FRAME.update({
    func_name: Function(
        TreeNode(Token(func_name, 'id', (func_name,))),
        DEFAULT_FRAME
    )
    for func_name in BUILTIN_FUNC_DOERS
})
