mems = {}
with open('memory_check_out.txt', 'r') as f:
    for line in f:
        free_alloc, code_pos, addr, size = line.rstrip().split(' ')
        if addr in mems:
            if free_alloc == 'free':
                del mems[addr]
            else:
                print('double alloc??')
        else:
            mems[addr] = (code_pos, size)
print('\n'.join([
    f'{code_pos} {addr} ({size})'
    for addr, (code_pos, size) in mems.items()
]))
