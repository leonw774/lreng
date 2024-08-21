from fractions import Fraction
from typing import Literal, Union, Optional

class Token:
    def __init__(
            self,
            raw: str,
            tok_type: Literal['id', 'num', 'op'],
            debug_pos_info: Optional[tuple] = None) -> None:
        """raw: token string
        tok_type: token type
        debug_pos_info: position (line num, col num) of the token"""
        self.raw = raw
        self.type = tok_type
        self.db_pos_info = debug_pos_info

    def __repr__(self) -> str:
        return f'T({self.type}:{repr(self.raw)}:{repr(self.db_pos_info)})'

class TreeNode:
    def __init__(self, tok: Token,
                 left = None, right = None) -> None:
        self.tok = tok
        self.left: TreeNode | None = left
        self.right: TreeNode | None = right

    def __repr__(self):
        return f'N({self.tok.type}:{repr(self.tok.raw)}:{self.tok.db_pos_info})'

    @classmethod
    def dump(cls, root_node, indent = 2) -> str:
        res_str = ''
        res_str += repr(root_node) + '\n'
        queue = []
        queue.append((root_node.right, indent))
        queue.append((root_node.left, indent))
        while len(queue) > 0:
            node, depth = queue.pop()
            if node is not None:
                res_str += ' ' * depth + repr(node) + '\n'
                queue.append((node.right, depth + indent))
                queue.append((node.left, depth + indent))
        return res_str


class Frame:
    def __init__(self, local: dict | None = None, source: Union['Frame', None] = None):
        if local is None:
            self._local: dict = dict()
        else:
            self._local: dict = local.copy()
        if source is None:
            self._shared: Union['Frame', dict] = dict()
        else:
            self._shared: Union['Frame', dict] = source

    def __getitem__(self, key):
        if key in self._local:
            return self._local[key]
        if key in self._shared:
            return self._shared[key]
        raise KeyError(f'Key {repr(key)} not found')

    def __setitem__(self, key, value):
        if key in self._shared:
            self._shared[key] = value
        else:
            self._local[key] = value

    def __contains__(self, key):
        return key in self._local or key in key in self._shared

    def __repr__(self):
        return repr(self.to_dict())

    def to_dict(self) -> dict:
        result = self._local.copy()
        if isinstance(self._shared, dict):
            result.update(self._shared)
        elif isinstance(self._shared, Frame):
            result.update(self._shared.to_dict())
        else:
            raise ValueError('self._shared is not dict or Frame')
        return result

    def get(self, key, value):
        if key in self._local:
            return self._local[key]
        if key in self._shared:
            return self._shared[key]
        return value


class GeneralObj:
    pass

class NullObj(GeneralObj):
    def __init__(self) -> None:
        self.value = None

    def __bool__(self) -> bool:
        return False

    def __repr__(self) -> str:
        return 'null'

    def __eq__(self, other) -> bool:
        return other.value is None

class NumObj(GeneralObj):
    def __init__(self, init_value: float | str | Fraction) -> None:
        self.value = Fraction(init_value)

    def __bool__(self) -> bool:
        return self.value != 0

    def __repr__(self) -> str:
        return (
            repr(int(self.value))
            if self.value.denominator == 1 else
            repr(float(self.value))
        )

    def __eq__(self, other) -> bool:
        return isinstance(other, NumObj) and self.value == other.value

class FuncObj(GeneralObj):
    def __init__(
            self,
            code_root_node: TreeNode,
            id_obj_table: Frame,
            arg_id: str | None = None) -> None:
        self.arg_id = arg_id
        self.code_root_node = code_root_node
        # the reference of the id-obj table at the same scope of the function
        # so that it can do recursion and access variable from outside
        self.id_obj_table = id_obj_table

    def __bool__(self) -> bool:
        return True

    def __repr__(self) -> str:
        return f'[function {self.arg_id} : {self.code_root_node}]'

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, FuncObj)
            and self.code_root_node == other.code_root_node
        )

class PairObj(GeneralObj):
    def __init__(self, init_left: GeneralObj, init_right: GeneralObj) -> None:
        self.left = init_left
        self.right = init_right

    def __bool__(self) -> bool:
        return True

    def __repr__(self) -> str:
        return f'({self.left}, {self.right})'

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, PairObj)
            and self.left == other.left
            and self.right == other.right
        )
