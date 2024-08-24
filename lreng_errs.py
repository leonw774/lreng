
def throw_syntax_err_msg(pos: tuple[int, int], msg: str):
    print(f'[SyntaxError]: Line {pos[0]} col {pos[1]}: {msg}')
    exit(1)

def print_semantic_err_msg(pos: tuple[int, int], msg: str):
    print(f'[SemanticError]: Line {pos[0]} col {pos[1]}: {msg}')
    exit(1)

def throw_runtime_err_msg(
        db_info: tuple[int, int] | tuple[str] | None,
        msg: str):
    if len(db_info) == 2:
        print(f'[RumtimeError]: Line {db_info[0]} col {db_info[1]}: {msg}')
    elif len(db_info) == 1:
        print(f'[RumtimeError]: In built-in function `{db_info[0]}`: {msg}')
    else:
        raise ValueError('Bad db_info')
    exit(2)
