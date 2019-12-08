#include <chrono>
#include <iostream>

#include "benchmark.h"
#include "utility.h"
#include "key_generator.h"

void backendTest() {
    const std::string base_path = "../traces/";

    TraceBenchmarkApp app;
    app.verbose  = false;
    app.log_file = "backends.csv";
    for (auto backend : {"lru", "deferred"}) {
        app.backend = backend;
        for (const TraceInfo& ti : {traces::mm, traces::lu, traces::zipf, traces::wiki}) {
            app.trace_file = base_path + ti.filename;
            for (size_t capacity = ti.items; capacity >= std::max(ti.items / 1024, 10lu);
                 capacity /= 2) {
                app.capacity = capacity;
                app.run();
            }
        }
    }
}

void hyperparameterTest() {

    const std::string base_path = "../traces/";

    TraceBenchmarkApp app;
    app.verbose  = false;
    app.log_file = "xhyper.csv";
    app.backend  = "deferred";

    for (const TraceInfo& ti : {traces::zipf, traces::wiki}) {
        app.trace_file = base_path + ti.filename;
        for (size_t capacity : {ti.items / 10, ti.items / 100}) {
            app.capacity = capacity;
            for (float pull : {0.01, 0.1, 0.2, 0.3}) {
                app.pull_threshold = pull;
                for (float purge : {0.0001, 0.0005, 0.002, 0.01}) {
                    app.purge_threshold = purge;
                    app.run();
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    TraceBenchmarkApp app;

    bool cl_mode = false; //(argc > 1);
    if (cl_mode) {
        auto rc = app.parse(argc, argv);
        if (rc) {
            return rc;
        }
        app.run();
    } else {
        // backendTest();
        hyperparameterTest();
    }

    return 0;
}
