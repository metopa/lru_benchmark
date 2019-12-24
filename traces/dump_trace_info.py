from pathlib import Path
from struct import unpack, calcsize


# [version] [requests] [requests] [unique] [data]
#         1          x          x        z  index
#         2 file size       count   unique  (i, s)

if __name__ == '__main__':
    p = Path('.')
    if (p / 'traces').exists():
        p = p / 'traces'

    for trace in p.glob('*.blis'):
        with trace.open('rb') as f:
            version, size, requests, unique = unpack('qqqq', f.read(8 * 4))
            print(f'{str(trace.name):<19} version = {version}   size = {size:>8}   '
                  f'requests/unique = {requests:>8}/{unique:>8} [{requests/unique:.3f}]')

