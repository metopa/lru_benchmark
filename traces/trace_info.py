from pathlib import Path
from struct import unpack, calcsize
import click
from tqdm import tqdm
import math
import numpy as np


class Trace:
    def __init__(self, filename):
        # [version] [requests] [requests] [unique] [data]
        #         1          x          x        z  index
        #         2 file size       count   unique  (i, s)
        self.filename = filename
        with filename.open('rb') as f:
            self.version, self.size, self.requests, self.unique = unpack('qqqq', f.read(8 * 4))
        if self.version not in (1, 2):
            raise RuntimeError(f"Unknown trace version {version}")

    def __iter__(self):
        with self.filename.open('rb') as f:
            f.read(8 * 4)
            if self.version == 1:
                for _ in range(self.size):
                    yield unpack('q', f.read(8))[0]
            elif self.version == 2:
                for _ in range(self.size):
                    start, count = unpack('qq', f.read(8 * 2))
                    yield from range(start, start + count)


@click.group()
def commands():
    pass


@commands.command(name='list', help='List all traces in a folder and their size')
@click.argument('path')
def list_all(path):
    p = Path(path)

    for filename in p.glob('*.blis'):
        trace = Trace(filename)
        print(f'{str(trace.filename.name):<19} version = {trace.version}   '
              f'size = {trace.size:>8}   '
              f'requests/unique = {trace.requests:>8}/{trace.unique:>8} '
              f'[{trace.requests / trace.unique:.3f}]')


@commands.command(help='Get info about single trace')
@click.argument('filename')
def stat(filename):
    trace = Trace(Path(filename))
    n = trace.requests - trace.unique

    last_seen = {}
    distances = []
    for i, req in tqdm(enumerate(iter(trace)), desc="Loading data",
                       total=trace.requests, leave=False, miniters=100000):
        d = i - last_seen.get(req, i)
        if d != 0:
            distances.append(d)
        last_seen[req] = i

    distances = np.array(distances)
    mean = np.mean(distances)
    stddev = distances.std()
    median = int(np.median(distances))

    print(f'{trace.filename.name}')
    print(f'Requests/Targets: {trace.requests}/{trace.unique} '
          f'[{trace.requests / trace.unique:.1f} R/T]')
    print(f'Median distance:  {median}')
    print(f'Mean distance:    {mean:.0f} ± {stddev:.0f} [RSD {stddev / mean:.3f}]')


if __name__ == '__main__':
    commands()
