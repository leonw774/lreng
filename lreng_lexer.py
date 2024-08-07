from string import ascii_letters, digits, printable, whitespace
from typing import Callable, Iterable, Literal, Optional, TypeAlias, TypeVar

from lreng_opers import (
    all_ops, R_BRACKETS, FUNC_CALL_L_PARENTHESE
)

unary_pm_preced = set(all_ops).difference(R_BRACKETS).union([None])

SINGLE_QUOTE = '\''
NULL_CHAR = '\0'
HASH_CHAR = '#'
id_chars = set(ascii_letters + digits + '_')
num_chars = set(digits + '.')
op_chars = set(r''.join(ops for ops in all_ops))
ws_chars = set(whitespace)

NUM_HEX_PREFIX = '0x'
NUM_BIN_PREFIX = '0b'
hex_chars = set(digits + 'ABCDEFabcdef')
bin_chars = set('01')

esc_table = {
    'a': '\a',
    'b': '\b',
    'n': '\n',
    'r': '\r',
    't': '\t',
    'v': '\v',
    '\\': '\\',
    '\'': '\''
}

class Token:
    def __init__(self, raw, tok_type: Literal['id', 'num', 'op']) -> None:
        self.raw = raw
        self.type = tok_type

    def __repr__(self) -> str:
        return f'T({self.type}:{repr(self.raw)})'


CargoTypeT = TypeVar('CargoTypeT')
StateType: TypeAlias = Callable[
    [Iterable, CargoTypeT],
    tuple[Optional['StateType'], CargoTypeT]
]
class StateMachine:
    '''Implementation taken from: https://gnosis.cx/TPiP/chap4.txt'''
    def __init__(self) -> None:
        self.states: Iterable[StateType] = set()

    def add_state(self, state_func: StateType):
        self.states.add(state_func)

    def run(
            self,
            iterator: Iterable,
            starter: StateType,
            cargo: CargoTypeT,
            is_debug=False) -> CargoTypeT:
        assert starter in self.states
        cur_state = starter
        while True:
            next_state, cargo = cur_state(iterator, cargo)
            if is_debug:
                print(*cargo, sep='\n')
            if next_state is None:
                return cargo
            elif next_state not in self.states:
                raise ValueError(f'Invalid state: {repr(next_state)}')
            else:
                cur_state = next_state

def parse_token(raw_str: str, is_debug=False) -> list[Token]:
    MyCargoType: TypeAlias = tuple[list[Token], str]

    def state_comment(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        if c == NULL_CHAR:
            return None, cargo
        elif c == '\n':
            return state_ws, cargo
        else:
            return state_comment, cargo

    def state_ws(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        out_list, ch_queue = cargo
        if c == NULL_CHAR:
            return None, cargo
        elif c == HASH_CHAR:
            return state_comment, cargo
        elif c in ws_chars:
            return state_ws, cargo
        elif c in num_chars:
            return state_num, (out_list, c)
        elif c == SINGLE_QUOTE:
            return state_char, (out_list, '')
        elif c in id_chars:
            return state_id, (out_list, c)
        elif c in op_chars:
            last = out_list[-1] if len(out_list) else None
            # handle unary + -
            if (c == '-' or c == '+') and last.raw in unary_pm_preced:
                ch_queue = '!' + c
            # handle function call
            elif (c == '(' and (last.type != 'op' or last.raw in R_BRACKETS)):
                ch_queue = FUNC_CALL_L_PARENTHESE
            else:
                # add inferred null in empty call
                if c == ')' and last.raw == '$(':
                    out_list.append(Token('null', 'id'))
                ch_queue = c
            return state_op, (out_list, ch_queue)

    def state_num(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        out_list, ch_queue = cargo
        if c == NULL_CHAR:
            if ch_queue.startswith((NUM_BIN_PREFIX, NUM_HEX_PREFIX)):
                ch_queue = str(int(ch_queue, base=0))
            out_list.append(Token(ch_queue, 'num'))
            return None, (out_list, '')
        elif c == HASH_CHAR:
            if ch_queue.startswith((NUM_BIN_PREFIX, NUM_HEX_PREFIX)):
                ch_queue = str(int(ch_queue, base=0))
            out_list.append(Token(ch_queue, 'num'))
            return state_comment, (out_list, '')
        elif c in ws_chars:
            if ch_queue.startswith((NUM_BIN_PREFIX, NUM_HEX_PREFIX)):
                ch_queue = str(int(ch_queue, base=0))
            out_list.append(Token(ch_queue, 'num'))
            return state_ws, (out_list, '')
        elif c in num_chars:
            if c == '.' and '.' in ch_queue:
                raise ValueError(f'Bad num charactor {ch_queue} + {repr(c)}')
            return state_num, (out_list, ch_queue + c)
        elif c in id_chars:
            is_good_format = False
            if ch_queue + c == NUM_HEX_PREFIX or ch_queue + c == NUM_BIN_PREFIX:
                # is begin of hex or binary number: '0x' or '0b'
                is_good_format = True
            elif ch_queue[:2] == NUM_HEX_PREFIX and c in hex_chars:
                # is part of hex number
                is_good_format = True
            if not is_good_format:
                raise ValueError(f'Bad num charactor {ch_queue} + {repr(c)}')
            return state_num, (out_list, ch_queue + c)
        elif op_chars:
            if ch_queue.startswith((NUM_BIN_PREFIX, NUM_HEX_PREFIX)):
                ch_queue = str(int(ch_queue, base=0))
            out_list.append(Token(ch_queue, 'num'))
            return state_op, (out_list, c)
        else:
            raise ValueError(f'Bad num charactor {ch_queue} + {repr(c)}')

    def state_char(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        out_list, ch_queue = cargo
        if len(ch_queue) == 0:
            if c == SINGLE_QUOTE:
                raise ValueError(f'Bad ascii charactor format: {repr(c)}')
            elif c in printable:
                return state_char, (out_list, c)
            else:
                raise ValueError(f'Bad ascii charactor: {repr(c)}')
        elif len(ch_queue) == 1:
            if ch_queue == '\\':
                if c in esc_table:
                    return state_char, (out_list, esc_table[c])
                else:
                    raise ValueError(f'Bad escape charactor: {repr(c)}')
            elif c == SINGLE_QUOTE:
                out_list.append(Token(ord(ch_queue), 'num'))
                return state_char, (out_list, ch_queue + c)
            else:
                raise ValueError(f'Bad ascii charactor format: {repr(c)}')
        elif len(ch_queue) == 2 and ch_queue[1] == SINGLE_QUOTE:
            if c == NULL_CHAR:
                return None, (out_list, '')
            elif c in ws_chars:
                return state_ws, (out_list, '')
            elif c in num_chars:
                return state_num, (out_list, c)
            elif c in id_chars:
                return state_id, (out_list, c)
            elif c in op_chars:
                return state_op, (out_list, c)
        else:
            raise ValueError(f'Bad ascii charactor format: {repr(c)}')

    def state_id(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        out_list, ch_queue = cargo
        if c == NULL_CHAR:
            out_list.append(Token(ch_queue, 'id'))
            return None, (out_list, '')
        elif c == HASH_CHAR:
            out_list.append(Token(ch_queue, 'id'))
            return state_comment, (out_list, '')
        elif c in ws_chars:
            out_list.append(Token(ch_queue, 'id'))
            return state_ws, (out_list, '')
        elif c in num_chars:
            return state_id, (out_list, ch_queue + c)
        elif c in id_chars:
            return state_id, (out_list, ch_queue + c)
        elif c in op_chars:
            out_list.append(Token(ch_queue, 'id'))
            # handle function call
            if c == '(':
                return state_op, (out_list, FUNC_CALL_L_PARENTHESE)
            else:
                return state_op, (out_list, c)

    def state_op(
            str_iter: Iterable,
            cargo: MyCargoType) -> tuple[StateType | None, MyCargoType]:
        c = next(str_iter)
        out_list, ch_queue = cargo
        if c == NULL_CHAR:
            out_list.append(Token(ch_queue, 'op'))
            return None, (out_list, '')
        elif c == HASH_CHAR:
            out_list.append(Token(ch_queue, 'op'))
            return state_comment, (out_list, '')
        elif c in ws_chars:
            out_list.append(Token(ch_queue, 'op'))
            return state_ws, (out_list, '')
        elif c in num_chars:
            out_list.append(Token(ch_queue, 'op'))
            return state_num, (out_list, c)
        elif c == '\'':
            out_list.append(Token(ch_queue, 'op'))
            return state_char, (out_list, '')
        elif c in id_chars:
            out_list.append(Token(ch_queue, 'op'))
            return state_id, (out_list, c)
        elif op_chars:
            if ch_queue + c in all_ops:
                return state_op, (out_list, ch_queue + c)
            else:
                out_list.append(Token(ch_queue, 'op'))
                # handle unary + -
                if (c == '-' or c == '+') and ch_queue not in R_BRACKETS:
                    ch_queue = '!' + c
                # handle function call
                elif c == '(' and ch_queue in R_BRACKETS:
                    ch_queue = FUNC_CALL_L_PARENTHESE
                else:
                    # add the inferred null
                    if c == ')' and ch_queue == '$(':
                        out_list.append(Token('null', 'id'))
                    ch_queue = c
                return state_op, (out_list, ch_queue)


    raw_str = raw_str.strip() + '\0'
    sm = StateMachine()
    sm.add_state(state_comment)
    sm.add_state(state_ws)
    sm.add_state(state_num)
    sm.add_state(state_char)
    sm.add_state(state_id)
    sm.add_state(state_op)
    out_list: list[Token] = []
    ch_queue: str = ''
    final_cargo = sm.run(
        iter(raw_str),
        starter=state_ws,
        cargo=(out_list, ch_queue),
        is_debug=is_debug
    )
    out_list, ch_queue = final_cargo
    return out_list


if __name__ == '__main__':
    print(parse_token('+3 + 4 * -2 / (1-5) ^ 2 ^ 3'))
    print(parse_token('sin(max(2, 3) / 3 * pi)'))
    print(parse_token('add = p : { @p + $p ;}'))
