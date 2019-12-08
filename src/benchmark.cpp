//
// Created by metopa on 20/03/19.
//

#include <chrono>
#include <containers/bucketed_adapter.h>

#include "CLI11.hpp"

#include "benchmark.h"
#include "containers/concurrent_lru.h"
#include "containers/deferred_lru.h"
#include "containers/dummy.h"
#include "containers/hash_fixed.h"
#include "containers/hhvm_lru.h"
#include "containers/tbb_hash.h"
#include "containers/tbb_lru.h"
#include "csv_logger.h"
#include "random_key_generator.h"

class Payload {
  public:
    Payload(int level, uint64_t user_data_) : level_(level), user_data_(user_data_) {}

    lru_value_t operator()() const { return lru_value_t{{fibonacci(level_), user_data_}}; }

  private:
    uint64_t fibonacci(int n) const {
        if (n <= 1) {
            return 1;
        }
        return fibonacci(n - 1) + fibonacci(n - 2);
    }

    volatile int level_;
    uint64_t     user_data_;
};

template <typename Container>
void benchmark(RandomBenchmarkApp& b, Container& cont, CsvLogger& logger, int time_limit);

RandomBenchmarkApp::RandomBenchmarkApp()
    : app(help(), "LRU Benchmark"), payload_level(5), threads(1), iterations(1),
      limit_max_key(false), is_item_capacity(false), capacity(0), pull_threshold(0.1),
      purge_threshold(0.1), verbose(false), print_freq(1000), time_limit(60), profile(false) {
    app.add_option("--log-file,-L", log_file)->required();
    app.add_option("--name,-N", run_name)->required();
    app.add_option("--info,-I", run_info);
    app.add_option("--generator,-G", generator)->required();
    app.add_flag("--verbose,-v", verbose);
    app.add_set_ignore_case("--backend,-B", backend,
                            {"dummy", "hash", "lru", "concurrent", "deferred", "tbb", "tbb_hash",
                             "hhvm", "b_lru", "b_concurrent", "b_deferred"})
        ->required();
    app.add_option("--threads,-t", threads, "", true)->default_val("1");
    auto c = app.add_option("--capacity, -c", capacity);
    c->check([&](auto& s) {
        is_item_capacity = true;
        return std::string();
    });
    auto m = app.add_option("--memory, -m", capacity);
    m->check([&](auto& s) {
        is_item_capacity = false;
        return std::string();
    });
    c->excludes(m);
    app.add_option("--iterations,-i", iterations)->required();
    app.add_option("--print-freq,-q", print_freq);
    app.add_option("--payload,-p", payload_level);
    app.add_option("--fix-max-key", limit_max_key);
    app.add_option("--pull-thrs", pull_threshold);
    app.add_option("--purge-thrs", purge_threshold);
    app.add_option("--time-limit", time_limit);
    app.add_flag("--profile", profile);
}

const char* RandomBenchmarkApp::help() {
    return "LRU container benchmark. Minimal set of arguments is:\n"
           "  -L <log> -N <name> -B <backend> -i <iter count> -m <mem size> -a <area>";
}

int RandomBenchmarkApp::parse(int argc, char** argv) {
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    };
}

void RandomBenchmarkApp::run() {
    runImpl<false>();
}

template <bool EnableProfile>
void RandomBenchmarkApp::runImpl() {
    try {
        using config_t = ContainerConfig<lru_key_t, lru_value_t, std::hash<lru_key_t>, std::less<>,
                                         OpenMPLock, EmptyDeletePolicy, 4, false, EnableProfile>;

        CsvLogger l(log_file, verbose);

        volatile size_t tmp = capacity;
        capacity            = tmp;

        if (backend == "dummy") {
            DummyCache<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "hash") {
            HashFixed<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "lru") {
            LRUCache<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "concurrent") {
            ConcurrentLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "deferred") {
            DeferredLRU<config_t> lru(capacity, is_item_capacity, pull_threshold, purge_threshold);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "tbb") {
            TbbLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "tbb_hash") {
            TbbHash<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "hhvm") {
            HhvmLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "b_lru") {
            BucketedLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "b_concurrent") {
            BucketedConcurrentLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else if (backend == "b_deferred") {
            BucketedDeferredLRU<config_t> lru(capacity, is_item_capacity);
            benchmark(*this, lru, l, time_limit);
        } else {
            throw std::runtime_error("Unknown backend: " + backend);
        }
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}

template <typename Container>
void benchmark(RandomBenchmarkApp& b, Container& cont, CsvLogger& logger, int time_limit) {
    using std::chrono::duration;
    auto expected_payload = Payload(b.payload_level, 0)()[0];

    auto max_capacity =
        b.is_item_capacity
            ? cont.memStats().capacity
            : (cont.memStats().total_mem / (sizeof(lru_key_t) + sizeof(lru_value_t)));
    auto max_key = cont.memStats().capacity / 100 * 99;

    auto generator = KeyGenerator::factory(b, b.generator, max_key);

    std::chrono::system_clock::time_point start;

    bool cancel_flag = false;

    size_t passed_iterations = 0;
    size_t total_hits = 0;

#pragma omp parallel num_threads(b.threads) \
    shared(generator, b, cont, start, cancel_flag, passed_iterations)
    {
        auto private_gen = generator->clone();
        private_gen->setThread(omp_get_thread_num(), omp_get_num_threads());

#pragma omp single
        { start = std::chrono::system_clock::now(); };

        size_t iter = 0;
        size_t hits = 0;

        for (; iter < b.iterations; iter++) {
            lru_key_t   key = private_gen->getKey();
            lru_value_t value;
            lru_value_t expected_value = lru_value_t{{expected_payload, key}};
            if (cont.consumeCachedOrCompute(key, Payload(b.payload_level, key), value)) {
                hits++;
            }
            if (value != expected_value) {
                std::cerr << "Wrong value: " << value << " != " << expected_value << std::endl;
            }

            if (omp_get_thread_num() == 0 && iter % b.print_freq == 0) {
                std::cout << iter << '/' << b.iterations << '\r' << std::flush;
            }

            if (iter % 10000 == 0) {
                if (omp_get_thread_num() == 0) {
                    duration<double> dur = std::chrono::system_clock::now() - start;
                    if (dur.count() > time_limit) {
#pragma omp atomic write
                        cancel_flag = true;
                        break;
                    }
                } else {
                    bool private_cancel_flag = false;
#pragma omp atomic read
                    private_cancel_flag = cancel_flag;
                    if (private_cancel_flag) {
                        break;
                    }
                }
            }
        }

#pragma omp atomic update
        passed_iterations += iter;

#pragma omp atomic update
        total_hits += hits;
    }

    auto             stop = std::chrono::system_clock::now();
    duration<double> dur  = stop - start;

    logger.log(b.run_name, b.run_info, b.threads, b.payload_level, generator, cont,
               passed_iterations, total_hits, dur, b.pull_threshold, b.purge_threshold);
    // cont.memStats().print(std::cout);
}

template <typename Container>
void traceBenchmark(TraceBenchmarkApp& b, Container& cont, TraceCsvLogger& logger);

TraceBenchmarkApp::TraceBenchmarkApp()
    : app(help(), "Trace Benchmark"), iterations(1), capacity(0), pull_threshold(0.1),
      purge_threshold(0.1), verbose(false) {
    app.add_option("--log-file,-L", log_file)->required();
    app.add_option("--trace-file,-t", trace_file)->required();
    app.add_flag("--verbose,-v", verbose);
    app.add_set_ignore_case("--backend,-B", backend,
                            {"dummy", "hash", "lru", "concurrent", "deferred", "tbb", "tbb_hash",
                             "hhvm", "b_lru", "b_concurrent", "b_deferred"})
        ->required();
    app.add_option("--capacity, -c", capacity);
    app.add_option("--iterations,-i", iterations);
    app.add_option("--pull-thrs", pull_threshold);
    app.add_option("--purge-thrs", purge_threshold);
}

const char* TraceBenchmarkApp::help() {
    return "Trace benchmark. Minimal set of arguments is:\n"
           "  -L <log> -t <trace> -B <backend> -i <iter count> -c <capacity>";
}

int TraceBenchmarkApp::parse(int argc, char** argv) {
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    };
}

void TraceBenchmarkApp::run() {
    try {
        using config_t = ContainerConfig<lru_key_t, lru_value_t, std::hash<lru_key_t>, std::less<>,
                                         OpenMPLock, EmptyDeletePolicy, 4, false, true>;

        TraceCsvLogger l(log_file, verbose);

        volatile size_t tmp             = capacity;
        capacity                        = tmp;
        constexpr bool is_item_capacity = true;

        if (backend == "dummy") {
            DummyCache<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "hash") {
            HashFixed<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "lru") {
            LRUCache<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "concurrent") {
            ConcurrentLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "deferred") {
            DeferredLRU<config_t> lru(capacity, is_item_capacity, pull_threshold, purge_threshold);
            traceBenchmark(*this, lru, l);
        } else if (backend == "tbb") {
            TbbLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "tbb_hash") {
            TbbHash<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "hhvm") {
            HhvmLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "b_lru") {
            BucketedLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "b_concurrent") {
            BucketedConcurrentLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else if (backend == "b_deferred") {
            BucketedDeferredLRU<config_t> lru(capacity, is_item_capacity);
            traceBenchmark(*this, lru, l);
        } else {
            throw std::runtime_error("Unknown backend: " + backend);
        }
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}

const Trace& readTrace(const std::string& path) {
    static std::map<std::string, Trace> cache;
    auto                                it = cache.find(path);
    if (it == cache.end()) {
        std::ifstream file(path, std::ifstream::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Can't open " + path);
        }
        std::vector<uint64_t> data;
        std::vector<uint64_t> buffer;
        buffer.resize(10 * 1024 * 1024);
        while (file) {
            auto read_bytes = file.readsome(reinterpret_cast<char*>(buffer.data()),
                                            buffer.size() * sizeof(buffer[0]));
            if (read_bytes <= 0) {
                break;
            }
            data.insert(data.end(), buffer.data(),
                        buffer.data() + (read_bytes / sizeof(buffer[0])));
        }
        std::tie(it, std::ignore) = cache.emplace(path, Trace(std::move(data)));
    }
    if (it->second.data.empty()) {
        throw std::runtime_error("Something went wrong");
    }
    return it->second;
}

template <typename Container>
void traceBenchmark(TraceBenchmarkApp& b, Container& cont, TraceCsvLogger& logger) {
    using std::chrono::duration;

    auto& trace = readTrace(b.trace_file);

    std::chrono::system_clock::time_point start;

    start = std::chrono::system_clock::now();
    for (size_t iter = 0; iter < b.iterations + 1; iter++) {
        if (iter == 1) {
            cont.resetProfiler();
        }
        for (size_t i = 0; i < trace.data.size(); i++) {
            lru_key_t   key = trace.data[i];
            lru_value_t value;
            lru_value_t expected_value{{key, key}};

            cont.consumeCachedOrCompute(key, [=] { return lru_value_t{{key, key}}; }, value);
            if (value != expected_value) {
                std::cerr << "Wrong value: " << value << " != " << expected_value << std::endl;
            }

            if (i % 1000000 == 0) {
                std::cout << iter << '/' << i << '\r' << std::flush;
            }
        }
    }

    auto             stop = std::chrono::system_clock::now();
    duration<double> dur  = stop - start;

    logger.log("", b.trace_file, cont, trace.distinct_count, b.iterations, dur, b.pull_threshold,
               b.purge_threshold);
}
