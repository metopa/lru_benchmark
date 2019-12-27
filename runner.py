import copy
import os
import subprocess
import sys
from datetime import datetime
import struct
from pathlib import Path
from typing import Sequence, Any, Tuple, Union, List, Dict, Optional, Callable
from termcolor import colored

TIME_LIMIT = 3

REPS = list(range(1))
VERSION = 'GCP1'
ALL_CONTAINERS = ["tbb_hash", "lru", "concurrent", "deferred", "tbb",
                  "hhvm", "b_lru", "b_concurrent", "b_deferred"]

LRU_CONTAINERS = ALL_CONTAINERS[1:]
FAST_CONTAINERS = ["lru", "concurrent", "deferred", "hhvm", "b_lru", "b_concurrent", "b_deferred"]
BINNED_LRU_CONTAINERS = ALL_CONTAINERS[5:]
DLRU_CONTAINERS = ["deferred", "b_deferred"]
NODLRU_CONTAINERS = ["tbb_hash", "lru", "concurrent", "tbb", "hhvm", "b_lru", "b_concurrent"]
CURRENT_TEST = 'NA'


def main():
    traces_all = load_traces(Path('traces'))
    traces_main = [find_trace(traces_all, 'wiki'), find_trace(traces_all, 'P4'), find_trace(traces_all, 'P8')]
    threads_full = [1, 4, 8, 16, 24, 32]
    threads_96 = [32, 64, 96]
    threads_main = [1, 16, 32]
    threads_short = [1, 32]
    pull_push = [(0, 0), (0.001, 0.1), (0.1, 0.7), (0.99, 0.99)]

    capacity_main = [10, 1000]

    benchmarks = {
        'speedup': (lambda start: scalability(start, traces_main, capacity_main,
                                              threads_full, FAST_CONTAINERS, pull_push)),
        'speedup96': (lambda start: scalability(start, traces_main, capacity_main,
                                                threads_96, BINNED_LRU_CONTAINERS, pull_push)),
        'perf': (lambda start: scalability(start, traces_all, capacity_main,
                                           threads_main, LRU_CONTAINERS, pull_push)),
        'perf_nodlru': (lambda start: scalability(start, traces_all, capacity_main,
                                                  threads_main, NODLRU_CONTAINERS, pull_push)),
        'perf_dlru': (lambda start: scalability(start, traces_all, capacity_main,
                                                threads_main, DLRU_CONTAINERS, pull_push)),
        'meta': (lambda start: meta_parameters(start, traces_main, capacity_main, threads_short, True,
                                               [0.001, 0.01, 0.1, 0.4, 0.7, 0.9],
                                               [0.001, 0.01, 0.1, 0.4, 0.7, 0.9])),
        'meta2': (lambda start: meta_parameters(start, traces_main, capacity_main, threads_short, False,
                                                [0.75, 0.8, 0.9, 0.99], [0.75, 0.8, 0.9, 0.99])),
        'preflight': (lambda start: preflight_check(start, traces_all, ALL_CONTAINERS))
    }

    print('Available benchmarks:\n  ' + ' '.join(benchmarks.keys()))

    args = sys.argv[1:]
    if not args:
        print('Not benchmark specified')

    i = 0
    worklist = []
    while i < len(args):
        if i + 1 < len(args) and args[i + 1].startswith('+'):
            worklist.append((args[i], int(args[i + 1])))
            i += 2
        else:
            worklist.append((args[i], 0))
            i += 1

    for a, first_test in worklist:
        assert a in benchmarks

    for a, first_test in worklist:
        global CURRENT_TEST
        CURRENT_TEST = a
        benchmarks[a](first_test)

    return


def metaparam_filter(e: Dict) -> bool:
    try:
        if e['backend'] in DLRU_CONTAINERS:
            return e['pull_threshold'] != 0 and e['purge_threshold'] != 0
        return e['pull_threshold'] == 0 or e['purge_threshold'] == 0
    except KeyError:
        return True


def scalability(start, traces, capacity_factors, threads, containers, pull_purge, reps=3, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000)
    trace_worklist = generate_trace_worklist(traces, capacity_factors)

    app.run([
        ('reps', [1]),
        (('generator', 'capacity'), trace_worklist[:1]),
        ('threads', threads[:1]),
        ('backend', ['dummy']),
        ('time_limit', [20])
    ], 0)

    app.time_limit = TIME_LIMIT
    app.run([
        ('reps', list(range(reps))),
        (('generator', 'capacity'), trace_worklist),
        ('threads', threads),
        ('backend', containers),
        (('pull_threshold', 'purge_threshold'), pull_purge)
    ], start, metaparam_filter)


def preflight_check(start, traces, containers, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000,
                       pull_threshold=0.6, purge_threshold=0.6, time_limit=5)
    trace_worklist = generate_trace_worklist(traces, [2])

    app.run([
        (('generator', 'capacity'), trace_worklist),
        ('backend', ['dummy'])
    ])

    app.run([
        (('generator', 'capacity'), trace_worklist[:1]),
        ('backend', containers)
    ])


def meta_parameters(start, traces, capacity_factors, threads, reference, pull_step, purge_steps, reps=2, log_file=None):
    if log_file is None:
        log_file = CURRENT_TEST + '.csv'

    print(f'{"#" * 25}\n#### {CURRENT_TEST:^15} ####\n{"#" * 25}')

    app = BenchmarkApp(log_file=log_file, run_info=VERSION, print_freq=1000000)
    trace_worklist = generate_trace_worklist(traces, capacity_factors)

    app.run([
        ('reps', [1]),
        (('generator', 'capacity'), trace_worklist[:1]),
        ('threads', threads[-1:]),
        ('backend', ['dummy']),
        ('time_limit', [20])
    ], filter_predicate=metaparam_filter)

    app.time_limit = TIME_LIMIT

    if reference:
        app.run([
            ('reps', list(range(reps))),
            (('generator', 'capacity'), trace_worklist),
            ('threads', threads),
            ('backend', ['lru', 'hhvm'])
        ], filter_predicate=metaparam_filter)

    app.run([
        ('reps', list(range(reps))),
        (('generator', 'capacity'), trace_worklist),
        ('threads', threads),
        ('backend', ['deferred']),
        ('purge_threshold', purge_steps),
        ('pull_threshold', pull_step)
    ], start, filter_predicate=metaparam_filter)


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
            result.append((str(t.filename), t.unique_requests // f))

    return result


def find_trace(traces, name):
    res = list(filter(lambda t: name in str(t.filename), traces))
    if len(res) != 1:
        raise RuntimeError(f'Trace name {name} is ambiguous')
    return res[0]


SimpleOverride = Tuple[str, List[Any]]
CompoundOverride = Tuple[Tuple[str, ...], List[Tuple[Any, ...]]]


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

    def run(self, overrides: Sequence[Union[SimpleOverride, CompoundOverride]] = None, start=0,
            filter_predicate: Optional[Callable[[Dict], bool]] = None):
        if overrides is None:
            self.execute_benchmark()
            return

        experiments = list(self.generate_experiments(overrides, {}))
        if filter_predicate:
            experiments = list(filter(filter_predicate, experiments))

        old = copy.deepcopy(self.__dict__)

        for i, e in enumerate(experiments):
            for k, v in e.items():
                setattr(self, k, v)
            e_str = ', '.join(f'{k}={v}' for k, v in e.items())
            if i < start:
                print(colored(f'\n[{i + 1:>3}/{len(experiments)}]>> {e_str}', 'yellow', attrs=['bold']))
            else:
                print(colored(f'\n[{i + 1:>3}/{len(experiments)}]>> {e_str}', 'blue', attrs=['bold']))
                self.execute_benchmark()

        self.__dict__ = old

    def generate_experiments(self, overrides: Sequence[Union[SimpleOverride, CompoundOverride]],
                             accumulator: Dict[str, Any]):
        if not overrides:
            yield copy.deepcopy(accumulator)
        else:
            kk, vv = overrides[0]
            if isinstance(kk, str):
                kk = (kk,)
                vv = [(v,) for v in vv]

            for v in vv:
                for k, x in zip(kk, v):
                    accumulator[k] = x
                yield from self.generate_experiments(overrides[1:], accumulator)

    def execute_benchmark(self):
        def ask_user():
            print(
                f'Do you want to {colored("[r]estart", "yellow")}, '
                f'{colored("[s]kip", "blue")} or {colored("[e]xit", "red")}? ')
            while True:
                choice = input()
                if choice == 's':
                    return True
                elif choice == 'e':
                    exit(1)
                elif choice == 'r':
                    return False

        args = [self.app_path,
                '-L', self.log_file,
                '-N', self.run_name,
                '-I', self.run_info,
                '-G', self.generator,
                '-v',
                '-B', self.backend,
                '-t', self.threads,
                '-c' if self.is_item_capacity else '-m', max(self.capacity, 4096),
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

        while True:
            try:
                result = subprocess.run(args, timeout=TIME_LIMIT * 10)
                if result.returncode != 0:
                    print('Return code: ' + str(result.returncode))
                    if ask_user():
                        break
                break
            except subprocess.CalledProcessError as e:
                print(e, file=os.stderr)
            except subprocess.TimeoutExpired as e:
                print("TIMEOUT")
                if ask_user():
                    break
            except KeyboardInterrupt:
                if ask_user():
                    break


def get_run_name():
    return datetime.now().strftime('%H:%M:%S')


if __name__ == '__main__':
    main()
