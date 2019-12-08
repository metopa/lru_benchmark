#pragma once

#include <string>

#include "CLI11.hpp"

struct Trace {
    std::vector<uint64_t> data;
    uint64_t distinct_count;

    explicit Trace(std::vector<uint64_t> data_args) : data(std::move(data_args)) {
        distinct_count = std::set<uint64_t>(data.begin(), data.end()).size();
    }
};

const Trace& readTrace(const std::string& path);

struct RandomBenchmarkApp {
    CLI::App    app;
    std::string log_file;
    std::string run_name;
    std::string run_info;
    std::string generator;
    std::string backend;
    int         payload_level;
    unsigned    threads;
    size_t      iterations;
    bool        limit_max_key;
    bool        is_item_capacity;
    size_t      capacity;
    double      pull_threshold;
    double      purge_threshold;
    bool        verbose;
    bool        profile;
    size_t      print_freq;
    int         time_limit;

    RandomBenchmarkApp();

    static const char* help();

    int parse(int argc, char** argv);

    void run();

  private:
    template <bool EnableProfile>
    void runImpl();
};


struct TraceBenchmarkApp {
    CLI::App app;
    std::string log_file;
    std::string trace_file;
    std::string backend;
    size_t iterations;
    size_t capacity;
    double pull_threshold;
    double purge_threshold;
    bool verbose;

    TraceBenchmarkApp();

    static const char* help();

    int parse(int argc, char** argv);

    void run();
};
