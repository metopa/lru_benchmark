import os
import subprocess
import sys
from datetime import datetime
import struct
from functools import reduce
from pathlib import Path
from typing import Sequence, Any, Tuple, Union, List
from termcolor import colored

TIME_LIMIT = 60

REPS = list(range(1))
VERSION = 'GCP1'
ALL_CONTAINERS = ["tbb_hash", "lru", "concurrent", "deferred", "tbb",
                  "hhvm", "b_lru", "b_concurrent", "b_deferred"]

LRU_CONTAINERS = ALL_CONTAINERS[1:]
FAST_CONTAINERS = ["lru", "concurrent", "deferred", "hhvm", "b_lru", "b_concurrent", "b_deferred"]
BINNED_LRU_CONTAINERS = ALL_CONTAINERS[5:]
DLRU_CONTAINERS = ["deferred", "b_deferred"]
CURRENT_TEST = 'NA'


def main():
    traces_all = load_traces(Path('traces'))
    traces_main = [find_trace(traces_all, 'wiki'), find_trace(traces_all, 'P4'), find_trace(traces_all, 'P8')]
    threads_full = [1, 4, 8, 16, 24, 32]
    threads_96 = [32, 64, 96]
    threads_main = [1, 16, 32]
    threads_short = [1, 32]
    optimal_pull = 0.7
    optimal_push = 0.7

    capacity_main = [10, 1000]

    benchmarks = {
        'speedup': (lambda: scalability(traces_main, capacity_main,
                                        threads_full, FAST_CONTAINERS,
                                        optimal_pull, optimal_push)),
        'speedup96': (lambda: scalability(traces_main, capacity_main,
                                          threads_96, BINNED_LRU_CONTAINERS,
                                          optimal_pull, optimal_push)),
        'perf': (lambda: scalability(traces_all, capacity_main,
                                     threads_main, LRU_CONTAINERS,
                                     optimal_pull, optimal_push)),
        'meta': (lambda: meta_parameters(traces_main, capacity_main, threads_short)),
        'preflight': (lambda: preflight_check(traces_all, ALL_CONTAINERS))
    }

    print('Available benchmarks:\n  ' + ' '.join(benchmarks.keys()))

    args = sys.argv[1:]
    if not args:
        print('Not benchmark specified')

    for a in args:
        assert a in benchmarks

    for a in args:
        global CURRENT_TEST
        CURRENT_TEST = a
        benchmarks[a]()

    return


def scalability(traces, capacity_factors, threads, containers, pull, purge, reps=3, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000,
                       pull_threshold=pull, purge_threshold=purge)
    trace_worklist = generate_trace_worklist(traces, capacity_factors)

    app.run([
        ('reps', [1]),
        (['generator', 'capacity'], trace_worklist[:1]),
        ('threads', threads[-1:]),
        ('backend', ['dummy']),
        ('time_limit', [20])
    ])

    app.time_limit = TIME_LIMIT
    app.run([
        ('reps', list(range(reps))),
        (['generator', 'capacity'], trace_worklist),
        ('threads', threads),
        ('backend', containers),
    ])


def preflight_check(traces, containers, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000,
                       pull_threshold=0.6, purge_threshold=0.6, time_limit=5)
    trace_worklist = generate_trace_worklist(traces, [2])

    app.run([
        (['generator', 'capacity'], trace_worklist),
        ('backend', ['dummy'])
    ])

    app.run([
        (['generator', 'capacity'], trace_worklist[:1]),
        ('backend', containers)
    ])


def meta_parameters(traces, capacity_factors, threads, reps=2, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000)
    trace_worklist = generate_trace_worklist(traces, capacity_factors)
    steps = [0.001, 0.01, 0.1, 0.4, 0.7, 0.9]

    app.run([
        ('reps', [1]),
        (['generator', 'capacity'], trace_worklist[:1]),
        ('threads', threads[-1:]),
        ('backend', ['dummy']),
        ('purge_threshold', steps[:1]),
        ('pull_threshold', steps[:1]),
        ('time_limit', [20])
    ])

    app.time_limit = TIME_LIMIT
    app.run([
        ('reps', list(range(reps))),
        (['generator', 'capacity'], trace_worklist),
        ('threads', threads),
        ('backend', ['lru', 'hhvm'])
    ])

    app.run([
        ('reps', list(range(reps))),
        (['generator', 'capacity'], trace_worklist),
        ('threads', threads),
        ('backend', ['deferred']),
        ('purge_threshold', steps),
        ('pull_threshold', steps)
    ])


class Trace:
    def __init__(self, filename: Path):
        self.filename = filename
        header_format = struct.Struct('qqqq')

        with filename.open('rb') as f:
            version, length, requests, unique = header_format.unpack(f.read(header_format.size))

        assert version in (1, 2)
        self.total_requests = requests
        self.unique_requests = unique


def load_traces(base_dir: Path):
    traces = sorted([Trace(f) for f in base_dir.glob("*.blis")], key=lambda x: x.filename)
    print('Loaded traces: ')
    print('\n'.join(f'  {str(t.filename)}' for t in traces))
    return traces


def generate_trace_worklist(traces, capacity_factors):
    result = []
    for t in traces:
        for f in capacity_factors:
            result.append([str(t.filename), t.unique_requests // f])

    return result


def find_trace(traces, name):
    res = list(filter(lambda t: name in str(t.filename), traces))
    if len(res) != 1:
        raise RuntimeError(f'Trace name {name} is ambiguous')
    return res[0]


class BenchmarkApp:
    def __init__(self,
                 app_path='./cmake-build-release/lru_benchmark',
                 log_file='default.csv',
                 run_name=None,
                 run_info='',
                 generator='same',
                 backend='hash',
                 payload_level=1,
                 threads=1,
                 limit_max_key=False,
                 is_item_capacity=True,
                 capacity=100,
                 pull_threshold=0.7,
                 purge_threshold=0.7,
                 verbose=True,
                 print_freq=50000,
                 time_limit=TIME_LIMIT,
                 reps=3,
                 profile=True):
        if run_name is None:
            run_name = get_run_name()
        self.app_path = app_path
        self.log_file = log_file
        self.run_name = run_name
        self.run_info = run_info
        self.generator = generator
        self.backend = backend
        self.payload_level = payload_level
        self.threads = threads
        self.limit_max_key = limit_max_key
        self.is_item_capacity = is_item_capacity
        self.capacity = capacity
        self.pull_threshold = pull_threshold
        self.purge_threshold = purge_threshold
        self.verbose = verbose
        self.print_freq = print_freq
        self.time_limit = time_limit
        self.reps = reps
        self.profile = profile
        print(colored(f'{log_file}|{run_name}|{run_info}', 'green', attrs=['bold']))

    def run(self, overrides: Sequence[Tuple[Union[str, List[str]], Union[List[Any], List[List[Any]]]]] = None):
        total_count = reduce((lambda x, y: x * y), (len(o[1]) for o in overrides))
        progress = [0, total_count]
        self.run_impl([], progress, overrides)

    def run_impl(self, changes, progress: List[int],
                 overrides: Sequence[Tuple[Union[str, List[str]], Union[List[Any], List[List[Any]]]]] = None):

        def wrap_list(x):
            return x if isinstance(x, list) else [x]

        old = self.__dict__

        if not overrides:
            if progress:
                progress[0] += 1
                print(colored(f'\n[{progress[0]:>3}/{progress[1]}]>> ' + ', '.join(changes), 'blue', attrs=['bold']))
            else:
                print(colored(f'\n>> ' + ', '.join(changes), 'blue', attrs=['bold']))
            self.execute_benchmark()
        else:
            this_level = overrides[0]
            if not isinstance(this_level[0], list):
                this_level = ([this_level[0]], [[x] for x in wrap_list(this_level[1])])
            keys, values = this_level
            for vv in values:
                for k, v in zip(keys, vv):
                    setattr(self, k, v)
                self.run_impl(changes + [f'{k}={v}' for k, v in zip(keys, vv)], progress, overrides[1:])

        self.__dict__ = old

    def execute_benchmark(self):
        args = [self.app_path,
                '-L', self.log_file,
                '-N', self.run_name,
                '-I', self.run_info,
                '-G', self.generator,
                '-v',
                '-B', self.backend,
                '-t', self.threads,
                '-c' if self.is_item_capacity else '-m', self.capacity,
                '-q', self.print_freq,
                '-p', self.payload_level,
                '--pull-thrs', self.pull_threshold,
                '--purge-thrs', self.purge_threshold,
                '--time-limit', self.time_limit
                ]
        if self.limit_max_key:
            args.append('--fix-max-key')
            args.append('1')

        if self.profile:
            args.append('--profile')

        args = [str(a) for a in args]
        print('  >> ' + ' '.join(args))
        try:
            subprocess.run(args)
        except subprocess.CalledProcessError as e:
            print(e, file=os.stderr)
        except KeyboardInterrupt:
            print(f'Do you want to {colored("[r]estart", "yellow")}, {colored("[s]kip", "blue")} or {colored("[e]xit", "red")}? ')
            while True:
                choice = input()
                if choice == 's':
                    break
                elif choice == 'e':
                    exit(1)
                elif choice == 'r':
                    self.execute_benchmark()
                    break


def get_run_name():
    return datetime.now().strftime('%H:%M:%S')


if __name__ == '__main__':
    main()
