from collections.abc import Callable, Iterable
from string import printable
from typing import TypeAlias, TypeVar

from lreng_opers import (
    ALL_OPS, R_BRACKETS, FUNC_CALL_L_PARENTHESE
)
from lreng_objs import Token
from lreng_chars import (
    NUM_BIN_PREFIX, NUM_HEX_PREFIX, HEX_CHARS, CHAR_ESC_TABLE,
    COMMENT_STARTER, WS_CHARS, ID_CHARS, NUM_CHARS, OP_CHARS
)
from lreng_errs import throw_syntax_err_msg

SINGLE_QUOTE = '\''
NULL_CHAR = '\0'

CargoTypeT = TypeVar('CargoTypeT')
State: TypeAlias = Callable
State: TypeAlias = Callable[
    [Iterable, CargoTypeT],
    tuple[State | None, CargoTypeT]
]
class StateMachine:
    '''Implementation taken from: https://gnosis.cx/TPiP/chap4.txt'''
    def __init__(self) -> None:
        self.states: Iterable[State] = set()

    def add_state(self, state_func: State):
        self.states.add(state_func)

    def run(
            self,
            iterator: Iterable,
            starter: State,
            cargo: CargoTypeT,
            is_debug: bool = False) -> CargoTypeT:
        assert starter in self.states
        cur_state = starter
        while True:
            ret = cur_state(iterator, cargo)
            try:
                next_state, cargo = ret
            except TypeError as e:
                print(ret)
                raise e
            if is_debug:
                print(*map(repr, cargo), sep='\n')
            if next_state is None:
                return cargo
            elif next_state not in self.states:
                raise ValueError(f'Invalid state: {repr(next_state)}')
            else:
                cur_state = next_state

def iter_str_with_position(s: str):
    line_num = 1
    col_num = 1
    for c in s:
        yield c, (line_num, col_num)
        if c == '\n':
            line_num += 1
            col_num = 1
        else:
            col_num += 1

def tokenizer(raw_str: str, is_debug: bool = False) -> list[Token]:
    MyCargoTypeType: TypeAlias = tuple[list[Token], str]

    def state_comment(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, _pos = next(str_pos_iter)
        if c == NULL_CHAR:
            return None, cargo
        elif c == '\n':
            return state_ws, cargo
        else:
            return state_comment, cargo

    def state_ws(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, pos = next(str_pos_iter)
        out_list, cq = cargo
        if c == NULL_CHAR:
            return None, cargo
        elif c == COMMENT_STARTER:
            return state_comment, cargo
        elif c in WS_CHARS:
            return state_ws, cargo
        elif c in NUM_CHARS:
            return state_num, (out_list, c)
        elif c == SINGLE_QUOTE:
            return state_char, (out_list, '')
        elif c in ID_CHARS:
            return state_id, (out_list, c)
        elif c in OP_CHARS:
            last = out_list[-1] if len(out_list) else None
            # handle unary + -
            if (c == '-' or c == '+') and (
                    last.raw is None or last.raw in (ALL_OPS - R_BRACKETS)):
                cq = '!' + c
            # handle function call
            elif (c == '(' and (last.type != 'op' or last.raw in R_BRACKETS)):
                cq = FUNC_CALL_L_PARENTHESE
            else:
                # add inferred null in empty call
                if c == ')' and last.raw == FUNC_CALL_L_PARENTHESE:
                    out_list.append(Token('null', 'id', None))
                cq = c
            return state_op, (out_list, cq)
        else:
            throw_syntax_err_msg(
                pos,
                f'Unrecognized character: {repr(c)}'
            )


    def state_num(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, (ln, coln) = next(str_pos_iter)
        out_list, cq = cargo
        tok_coln = coln - len(cq)
        if c == NULL_CHAR:
            out_list.append(Token(cq, 'num', (ln, tok_coln)))
            return None, (out_list, '')
        elif c == COMMENT_STARTER:
            out_list.append(Token(cq, 'num', (ln, tok_coln)))
            return state_comment, (out_list, '')
        elif c in WS_CHARS:
            out_list.append(Token(cq, 'num', (ln, tok_coln)))
            return state_ws, (out_list, '')
        elif c in NUM_CHARS:
            if c == '.' and '.' in cq:
                throw_syntax_err_msg(
                    (ln, coln),
                    f'Second dot ({repr(c)}) in number {cq}'
                )
            return state_num, (out_list, cq + c)
        elif c in ID_CHARS:
            is_good_format = False
            if cq + c == NUM_HEX_PREFIX or cq + c == NUM_BIN_PREFIX:
                # is begin of hex or binary number: '0x' or '0b'
                is_good_format = True
            elif cq[:2] == NUM_HEX_PREFIX and c in HEX_CHARS:
                # is part of hex number
                is_good_format = True
            if not is_good_format:
                throw_syntax_err_msg(
                    (ln, coln),
                    f'Bad num format after {cq} + {repr(c)}'
                )
            return state_num, (out_list, cq + c)
        elif OP_CHARS:
            out_list.append(Token(cq, 'num', (ln, tok_coln)))
            return state_op, (out_list, c)
        else:
            throw_syntax_err_msg(
                (ln, coln),
                f'Illegal character for number: {repr(c)}'
            )

    def state_char(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, (ln, coln) = next(str_pos_iter)
        out_list, cq = cargo
        tok_coln = coln - len(cq)
        if len(cq) == 0:
            if c == SINGLE_QUOTE:
                throw_syntax_err_msg(
                    (ln, coln),
                    f'Empty ascii character: {repr(c)}'
                )
            elif c in printable:
                return state_char, (out_list, c)
            else:
                throw_syntax_err_msg(
                    (ln, coln),
                    f'Unprintable ascii character: {repr(c)}'
                )
        elif len(cq) == 1:
            if cq == '\\':
                if c in CHAR_ESC_TABLE:
                    return state_char, (out_list, cq + c)
                else:
                    throw_syntax_err_msg(
                        (ln, tok_coln),
                        f'Invalid escape character: {repr(cq + c)}'
                    )
            elif c == SINGLE_QUOTE:
                # closing non-escape character with single quote
                out_list.append(Token(cq, 'chr', (ln, tok_coln)))
                return state_char, (out_list, cq + c)
            else:
                throw_syntax_err_msg(
                    (ln, tok_coln),
                    f'Expect a single quote to end character {repr(cq)}. '
                    f'Get: {repr(c)}'
                )
        elif len(cq) == 2 and cq[0] == '\\':
            # closing escape character with single quote
            out_list.append(Token(cq, 'chr', (ln, tok_coln)))
            if c != SINGLE_QUOTE:
                throw_syntax_err_msg(
                    (ln, coln),
                    f'Expect single quote to end escaped character {repr(cq)}. '
                    f'Get {repr(c)}'
                )
            return state_char, (out_list, cq + c)
        elif (len(cq) == 2 or len(cq) == 3) and cq.endswith(SINGLE_QUOTE):
            # ending char
            if c == NULL_CHAR:
                return None, (out_list, '')
            elif c in WS_CHARS:
                return state_ws, (out_list, '')
            elif c == COMMENT_STARTER:
                return state_comment, (out_list, '')
            elif c in NUM_CHARS:
                return state_num, (out_list, c)
            elif c in ID_CHARS:
                return state_id, (out_list, c)
            elif c in OP_CHARS:
                return state_op, (out_list, c)
            else:
                throw_syntax_err_msg(
                    (ln, tok_coln),
                    f'Unrecognized character: {repr(c)}'
                )
        else:
            throw_syntax_err_msg(
                (ln, tok_coln),
                f'Bad ascii character format: {repr(cq + c)}'
            )

    def state_id(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, (ln, coln) = next(str_pos_iter)
        out_list, cq = cargo
        tok_coln = coln - len(cq)
        if c == NULL_CHAR:
            out_list.append(Token(cq, 'id', (ln, tok_coln)))
            return None, (out_list, '')
        elif c == COMMENT_STARTER:
            out_list.append(Token(cq, 'id', (ln, tok_coln)))
            return state_comment, (out_list, '')
        elif c in WS_CHARS:
            out_list.append(Token(cq, 'id', (ln, tok_coln)))
            return state_ws, (out_list, '')
        elif c in NUM_CHARS:
            return state_id, (out_list, cq + c)
        elif c in ID_CHARS:
            return state_id, (out_list, cq + c)
        elif c in OP_CHARS:
            out_list.append(Token(cq, 'id', (ln, tok_coln)))
            # handle function call
            if c == '(':
                return state_op, (out_list, FUNC_CALL_L_PARENTHESE)
            else:
                return state_op, (out_list, c)
        else:
            throw_syntax_err_msg(
                (ln, tok_coln),
                f'Unrecognized character: {repr(c)}'
            )

    def state_op(
            str_pos_iter: Iterable,
            cargo: MyCargoTypeType) -> tuple[State | None, MyCargoTypeType]:
        c, (ln, coln) = next(str_pos_iter)
        out_list, cq = cargo
        tok_coln = coln - len(cq)
        if c == NULL_CHAR:
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return None, (out_list, '')
        elif c == COMMENT_STARTER:
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return state_comment, (out_list, '')
        elif c in WS_CHARS:
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return state_ws, (out_list, '')
        elif c in NUM_CHARS:
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return state_num, (out_list, c)
        elif c == '\'':
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return state_char, (out_list, '')
        elif c in ID_CHARS:
            out_list.append(Token(cq, 'op', (ln, tok_coln)))
            return state_id, (out_list, c)
        elif c in OP_CHARS:
            if cq + c in ALL_OPS:
                return state_op, (out_list, cq + c)
            else:
                out_list.append(Token(cq, 'op', (ln, tok_coln)))
                # handle unary + -
                if (c == '-' or c == '+') and cq not in R_BRACKETS:
                    cq = '!' + c
                # handle function call
                elif c == '(' and cq in R_BRACKETS:
                    cq = FUNC_CALL_L_PARENTHESE
                # add the inferred null
                else:
                    if c == ')' and cq == FUNC_CALL_L_PARENTHESE:
                        out_list.append(Token('null', 'id', None))
                    cq = c
                return state_op, (out_list, cq)
        else:
            throw_syntax_err_msg(
                (ln, tok_coln),
                f'Unrecognized character: {repr(c)}'
            )

    raw_str = raw_str.strip() + '\0'
    sm = StateMachine()
    sm.add_state(state_comment)
    sm.add_state(state_ws)
    sm.add_state(state_num)
    sm.add_state(state_char)
    sm.add_state(state_id)
    sm.add_state(state_op)
    out_list: list[Token] = []
    cq: str = ''
    final_cargo = sm.run(
        iter_str_with_position(raw_str),
        starter=state_ws,
        cargo=(out_list, cq),
        is_debug=is_debug
    )
    out_list, cq = final_cargo
    return out_list


if __name__ == '__main__':
    print(tokenizer('+3 + 4 * -2 / (1-5) ^ 2 ^ 3'))
    print(tokenizer('sin(max(2, 3) / 3 * pi)'))
    print(tokenizer('add = p : { `p + ~p }'))
