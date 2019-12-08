import os
import subprocess
from collections import namedtuple
from datetime import datetime
from typing import Sequence, Any, Tuple, Union, List
from termcolor import colored

TIME_LIMIT = 60
THREADS = [1, 16, 32]
REPS = list(range(3))
VERSION = 'GCP1'


def main():
    hit_rate()
    return
    meta_parameters()
    find_performance()
    lru_performance()
    trace_performance()


def find_performance():
    app = BenchmarkApp(log_file='bench_synth.csv', run_info=VERSION,
                       capacity=3000 * 1000, print_freq=1000000,
                       limit_max_key=True)
    app.run([
        ('generator', ['uniform', 'normal']),
        ('threads', THREADS[-1:]),
        ('backend', 'dummy'),
        ('time_limit', [20])
    ])
    app.run([
        ('reps', REPS),
        ('generator', ['uniform', 'normal']),
        ('threads', THREADS),
        ('backend', ALL_CONTAINERS),
    ])


def lru_performance():
    app = BenchmarkApp(log_file='bench_synth.csv', run_info=VERSION,
                       capacity=1000 * 1000, print_freq=100000)
    app.run([
        ('generator', ['varsame', 'disjoint']),
        ('threads', THREADS[-1:]),
        ('backend', 'dummy'),
        ('time_limit', [20])
    ])
    app.time_limit = TIME_LIMIT

    app.run([
        ('reps', REPS),
        ('generator', ['varsame', 'disjoint']),
        ('threads', THREADS),
        ('backend', LRU_CONTAINERS),
    ])


def trace_performance():
    app = BenchmarkApp(log_file='bench_trace.csv', run_info=VERSION, print_freq=1000000)
    app.run([
        (['generator', 'capacity'], [Traces.xzipf84]),
        ('threads', THREADS[-1:]),
        ('backend', 'dummy'),
        ('time_limit', [20])
    ])
    app.time_limit = TIME_LIMIT
    app.run([
        ('reps', REPS),
        (['generator', 'capacity'], [Traces.xzipf84, Traces.xzipf44, Traces.xwiki84, Traces.xwiki44]),
        ('threads', THREADS),
        ('backend', LRU_CONTAINERS),
    ])


def trace_scaling():
    app = BenchmarkApp(log_file='bench_trace_96.csv', run_info=VERSION, print_freq=1000000, time_limit=TIME_LIMIT * 2)

    app.run([
        ('reps', REPS),
        (['generator', 'capacity'], [Traces.xwiki84, Traces.xwiki44]),
        ('threads', [1, 16, 32, 48, 64, 80, 96]),
        ('backend', ['b_deferred', "b_lru", "hhvm"]),
    ])


def meta_parameters():
    app = BenchmarkApp(log_file='bench_meta.csv', run_info=VERSION,
                       print_freq=1000000, threads=THREADS[-1], backend='deferred')
    STEPS = [0.01, 0.1, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
    app.run([
        (['generator', 'capacity'], [Traces.xzipf84, Traces.xwiki44]),
        ('reps', REPS),
        ('purge_threshold', STEPS),
        ('pull_threshold', STEPS),
    ])


def hit_rate():
    traces = [
        (Traces.zipf, 2), (Traces.zipf, 8), (Traces.zipf, 32), (Traces.zipf, 128),
        (Traces.wiki, 2), (Traces.wiki, 8), (Traces.wiki, 32), (Traces.wiki, 128),
        (Traces.mm, 2), (Traces.mm, 8), (Traces.mm, 32),
        (Traces.lu, 2), (Traces.lu, 8), (Traces.lu, 32),
    ]
    traces = [(t.trace, t.unique_count // f) for t, f in traces]
    app = BenchmarkApp(log_file='bench_hitrate.csv', run_info=VERSION,
                       print_freq=1000000, threads=THREADS[-1])
    app.run([
        (['generator', 'capacity'], traces),
        ('reps', [1]),
        ('threads', [1, 32]),
        ('backend', ['lru', 'deferred'])
    ])


ALL_CONTAINERS = ["tbb_hash", "lru", "concurrent", "deferred", "tbb",
                  "hhvm", "b_lru", "b_concurrent", "b_deferred"]

LRU_CONTAINERS = ALL_CONTAINERS[1:]
BINNED_LRU_CONTAINERS = ALL_CONTAINERS[5:]
DLRU_CONTAINERS = ["deferred", "b_deferred"]

TraceInfo = namedtuple('TraceInfo', ['trace', 'unique_count'])


class Traces:
    mm = TraceInfo('trace:mm', 1667)
    lu = TraceInfo('trace:lu', 531)
    zipf = TraceInfo('trace:zipf', 91356)
    wiki = TraceInfo('trace:wiki', 7913592)

    xzipf84 = [zipf.trace, zipf.unique_count // 2]
    xzipf44 = [zipf.trace, zipf.unique_count // 8]
    xwiki84 = [wiki.trace, wiki.unique_count // 8]
    xwiki44 = [wiki.trace, wiki.unique_count // 1000]


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
                 iterations=1000000000,
                 limit_max_key=False,
                 is_item_capacity=True,
                 capacity=100,
                 pull_threshold=0.7,
                 purge_threshold=0.7,
                 verbose=True,
                 print_freq=1000,
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
        self.iterations = iterations
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

    def run(self, overrides: Sequence[Tuple[Union[str, List[str]], Union[List[Any], List[List[Any]]]]] = None,
            changes=None):
        if changes is None:
            changes = []

        def wrap_list(x):
            return x if isinstance(x, list) else [x]

        old = self.__dict__

        if not overrides:
            print(colored('\n>> ' + ', '.join(changes), 'blue', attrs=['bold']))
            self.execute_benchmark()
        else:
            this_level = overrides[0]
            if not isinstance(this_level[0], list):
                this_level = ([this_level[0]], [[x] for x in wrap_list(this_level[1])])
            keys, values = this_level
            for vv in values:
                for k, v in zip(keys, vv):
                    setattr(self, k, v)
                self.run(overrides[1:], changes + [f'{k}={v}' for k, v in zip(keys, vv)])

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
                '-i', self.iterations,
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
            print('Do you want to [r]estart, [s]kip or [e]xit? ')
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


def get_worklist_for_trace(t: TraceInfo):
    sizes = [1 / 100, 1 / 1000, 1 / 10000] if t != Traces.lu else [1 / 2, 1 / 8]
    return [[t.trace, int(t.unique_count * s)] for s in sizes]


if __name__ == '__main__':
    main()
