#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
#include <omp.h>

void increment_mutex(int thread_count, size_t count) {
    std::mutex lock;
    size_t var = 0;
    std::chrono::high_resolution_clock::time_point start, stop;

    #pragma omp parallel shared(lock, var, start, stop, count) num_threads(thread_count)
    {
        #pragma omp barrier
        #pragma omp master
        {
            start = std::chrono::high_resolution_clock::now();
        }

        for (size_t i = 0; i < count; i++) {
            std::lock_guard guard{lock};
            var++;
        }

        #pragma omp barrier
        #pragma omp master
        {
            stop = std::chrono::high_resolution_clock::now();
        }
    }

    std::chrono::duration<float> dur = stop - start;
    std::cout << "Mutex: made " << (size_t)count * thread_count << " increments in " << dur.count() << "s\n";
    std::cout << "Actual variable value: " << var << std::endl;
}

void increment_atomic(int thread_count, size_t count) {
    std::atomic<size_t> var{0};
    std::chrono::high_resolution_clock::time_point start, stop;

    #pragma omp parallel shared(var, start, stop, count) num_threads(thread_count)
    {
        #pragma omp barrier
        #pragma omp master
        {
            start = std::chrono::high_resolution_clock::now();
        }

        for (size_t i = 0; i < count; i++) {
            var++;
        }

        #pragma omp barrier
        #pragma omp master
        {
            stop = std::chrono::high_resolution_clock::now();
        }
    }
    std::chrono::duration<float> dur = stop - start;
    std::cout << "AtomicINC: made " << (size_t)count * thread_count << " increments in " << dur.count() << "s\n";
    std::cout << "Actual variable value: " << var.load() << std::endl;
}

void increment_atomic_cas(int thread_count, size_t count) {
    std::atomic<size_t> var{0};
    std::chrono::high_resolution_clock::time_point start, stop;

    #pragma omp parallel shared(var, start, stop, count) num_threads(thread_count)
    {
        #pragma omp barrier
        #pragma omp master
        {
            start = std::chrono::high_resolution_clock::now();
        }

        for (size_t i = 0; i < count; i++) {
            size_t value = var.load(std::memory_order_relaxed);
            while (!var.compare_exchange_weak(value, value + 1))
                ;
        }

        #pragma omp barrier
        #pragma omp master
        {
            stop = std::chrono::high_resolution_clock::now();
        }
    }
    std::chrono::duration<float> dur = stop - start;
    std::cout << "AtomicCAS: made " << (size_t)count * thread_count << " increments in " << dur.count() << "s\n";
    std::cout << "Actual variable value: " << var.load() << std::endl;
}


int main() {
    int threads = 32;
    size_t count = 100000000;
    increment_mutex(threads, count);
    increment_atomic(threads, count);
    increment_atomic_cas(threads, count);
}
