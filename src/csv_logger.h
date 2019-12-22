#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "key_generator.h"

class CsvLogger {
  public:
    CsvLogger(const std::string& filename, bool verbose)
        : filename_(filename), verbose_(verbose),
          output_(filename, std::fstream::out | std::fstream::app) {
        if (!output_.is_open()) {
            throw std::runtime_error("Can't open log file!");
        }
        if (output_.tellp() == 0) {
            print_header(output_);
        }
    }

    template <typename Container>
    void log(const std::string& run_name, const std::string& run_tag, unsigned threads,
             int payload_level, const KeyGenerator::ptr_t& gen, Container& cont, size_t iterations,
             size_t hits, std::chrono::duration<double> duration, float pull_threshold,
             float purge_threshold, uint64_t unique_count, bool log_to_console = true,
             std::ostream* out = nullptr) {
        if (out == nullptr) {
            out = &output_;
        }

        auto mem               = cont.memStats();
        auto perf              = cont.profileStats();
        auto throughput        = iterations / duration.count();
        auto thread_throughput = throughput / threads;
        auto element_overhead  = cont.currentOverheadMemory() / std::max(mem.count, 1lu);
        // clang-format off
        *out <<
             run_name << ", " <<
             run_tag << ", " <<
             gen->name() << ", " <<
             cont.name() << ", " <<
             //mem.total_mem << ", " <<
             mem.capacity << ", " <<
             //iterations << ", " <<
             duration.count() << ", " <<
             threads << ", " <<
             throughput << ", " <<
             thread_throughput << ", " <<
             double(hits) / iterations << ", " <<
             //element_overhead << ", " <<
             //perf.find << ", " <<
             //perf.insert << ", " <<
             //perf.evict << ", " <<
             //perf.head_accesses << ", " <<
             //payload_level << ", " <<
             pull_threshold << ", " <<
             purge_threshold << "\n";
        // clang-format on
        if (log_to_console) {
            if (verbose_) {
                verbose_log(run_name, run_tag, threads, payload_level, gen, cont, iterations, hits,
                            duration, pull_threshold, purge_threshold, unique_count, &std::cout);
            } else {
                log(run_name, run_tag, threads, payload_level, gen, cont, iterations, hits,
                    duration, pull_threshold, purge_threshold, unique_count, false, &std::cout);
            }
        }
    }

  private:
    void print_header(std::ostream& stream) {
        stream << "test_name, test_tag, generator, container, "
                  //"available_memory, "
                  "capacity, "
                  //"iterations, "
                  "duration, "
                  "threads, "
                  "throughput, "
                  "thread_throughput, "
                  "hit_rate, "
                  //"overhead_per_elem, "
                  //"find, insert, evict, head_access, "
                  //"payload_level, "
                  "pull_threshold, purge_threshold"
                  "\n";
    }

    template <typename Container>
    void verbose_log(const std::string& run_name, const std::string& run_tag, unsigned threads,
                     int payload_level, const KeyGenerator::ptr_t& gen, Container& cont,
                     size_t iterations, size_t hits, std::chrono::duration<double> duration,
                     float pull_threshold, float purge_threshold, uint64_t unique_count,
                     std::ostream* out = nullptr) {
        const char* spacer = "     ";
        if (out == nullptr) {
            out = &output_;
        }

        auto mem               = cont.memStats();
        auto perf              = cont.profileStats();
        auto throughput        = iterations / duration.count();
        auto thread_throughput = throughput / threads;
        auto element_overhead  = cont.currentOverheadMemory() / std::max(mem.count, 1lu);
        //*out << "Output file:     " << spacer << filename_ << "\n";
        *out << "Backend/name/distribution: " << spacer << cont.name() << "/" << run_name;
        if (!run_tag.empty()) {
            *out << " (" << run_tag << ")";
        }
        *out << "/" << gen->name() << '\n';
        if (unique_count) {
            *out << "Memory/capacity:           " << spacer << prettyPrintSize(mem.total_mem) << "/"
                 << mem.capacity << " [" << (mem.capacity * 100. / unique_count) << "% of total]\n";
        } else {
            *out << "Memory/capacity:           " << spacer << prettyPrintSize(mem.total_mem) << "/"
                 << mem.capacity << "\n";
        }
        *out << "Thresholds:                " << spacer << pull_threshold << "/" << purge_threshold
             << "\n";
        //*out << "F/I/E/HA/HR:               " << spacer << (perf.find - perf.insert) / threads <<
        //"/"
        //     << perf.insert / threads << "/" << perf.evict / threads << "/"
        //     << perf.head_accesses / threads << "/" << (1 - perf.insert / double(perf.find)) * 100
        //     << "%\n";
        *out << "Thread throughput:         " << spacer << thread_throughput / 1000 << " kOp/s\n";
        *out << "Hit rate:                  " << spacer << double(hits) / iterations * 100 << "%\n";
    }

    std::string  filename_;
    bool         verbose_;
    std::fstream output_;
};

class TraceCsvLogger {
  public:
    TraceCsvLogger(const std::string& filename, bool verbose)
        : filename_(filename), verbose_(verbose),
          output_(filename, std::fstream::out | std::fstream::app) {
        if (!output_.is_open()) {
            throw std::runtime_error("Can't open log file!");
        }
        if (output_.tellp() == 0) {
            print_header(output_);
        }
    }

    template <typename Container>
    void log(const std::string& run_name, const std::string& trace_name, Container& cont,
             size_t item_count, size_t iterations, std::chrono::duration<double> duration,
             float pull, float purge, bool log_to_console = true, std::ostream* out = nullptr) {
        if (out == nullptr) {
            out = &output_;
        }

        auto mem        = cont.memStats();
        auto perf       = cont.profileStats();
        auto throughput = iterations / duration.count();
        // clang-format off
        *out <<
             trace_name << ", " <<
             cont.name() << ", " <<
             mem.capacity << ", " <<
             item_count << ", " <<
             perf.find << ", " <<
             (perf.find - perf.insert) << ", " <<
             (perf.find - perf.insert) * 100. / perf.find << ", " <<
             duration.count() << ", " <<
             throughput << ", " <<
             perf.insert << ", " <<
             perf.evict << ", " <<
             perf.head_accesses << ", " <<
             pull << ", " << purge << "\n";
        // clang-format on
        if (log_to_console) {
            if (verbose_) {
                verbose_log(run_name, trace_name, cont, item_count, iterations, duration,
                            &std::cout);
            } else {
                log(run_name, trace_name, cont, item_count, iterations, duration, pull, purge,
                    false, &std::cout);
            }
        }
    }

  private:
    void print_header(std::ostream& stream) {
        stream << "trace_name, container, capacity, "
                  "items, accesses, hits, hit_rate, "
                  "duration, throughput,"
                  "insert, evict, head_access, pull_threshold, purge_threshold\n";
    }

    template <typename Container>
    void verbose_log(const std::string& run_name, const std::string& trace_name, Container& cont,
                     size_t item_count, size_t iterations, std::chrono::duration<double> duration,
                     std::ostream* out = nullptr) {
        const char* spacer = "     ";
        if (out == nullptr) {
            out = &output_;
        }

        auto mem              = cont.memStats();
        auto perf             = cont.profileStats();
        auto throughput       = iterations / duration.count();
        auto element_overhead = cont.currentOverheadMemory() / std::max(mem.count, 1lu);
        *out << "Backend/Trace/Log:  " << spacer << cont.name() << "/" << trace_name << "/"
             << filename_ << '\n';
        *out << "Cap/Items/Accesses: " << spacer << mem.capacity << "/" << item_count << '/'
             << perf.find << "\n";
        *out << "Hits:               " << spacer << (perf.find - perf.insert) << " ("
             << (perf.find - perf.insert) * 100. / perf.find << "%)\n";
        *out << "Duration:           " << spacer << duration.count() << " s\n";
    }

    std::string  filename_;
    bool         verbose_;
    std::fstream output_;
};
