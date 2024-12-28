import sys

filename = sys.argv[1]
mems = dict()
funcs = dict()
with open(filename, 'rb') as f:
    for linenum, line in enumerate(f):
        if not line.startswith(b'[memcheck]'):
            continue
        strip_line = line.rstrip()[len(b'[memcheck]'):]
        try:
            func, code_pos, addr, *info = strip_line.split(b' ')
        except Exception as e:
            print(strip_line)
            raise e
        if func == b'[free]':
            if addr in mems:
                del mems[addr]
            else:
                print(linenum)
                print('not alloced free??')
                print(strip_line)
        elif func.endswith(b'alloc]'):
            if addr in mems:
                print(linenum)
                print('double alloc??')
                print(strip_line)
            else:
                mems[addr] = (linenum+1, func, code_pos, info)

print('\n'.join([
    f'{linenum}\n{func.decode()} {code_pos.decode()} '
    f'{addr.decode()} {", ".join(info_elem.decode() for info_elem in info)}'
    for addr, (linenum, func, code_pos, info) in mems.items()
]))
