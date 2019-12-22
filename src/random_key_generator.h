#pragma once

#include <iostream>
#include <random>

#include "common.h"

class ExponentialKeyGenerator {
  public:
    ExponentialKeyGenerator(size_t seed, size_t interval, double area_under_interval,
                            lru_key_t max_key)
        : interval_(interval), area_under_interval_(area_under_interval), max_key_(max_key),
          dist_(get_exp_lambda_for_area(interval, area_under_interval)), gen_(seed) {}

    lru_key_t operator()() { return std::min(static_cast<lru_key_t>(dist_(gen_)), max_key_); }

    size_t interval() const { return interval_; }

    double area_under_interval() const { return area_under_interval_; }

    double dist_parameter() const { return dist_.lambda(); }

    const char* dist_name() const { return "Exp"; }

    double get_F_for_interval(size_t interval) const {
        return 1 - std::exp(-dist_parameter() * interval);
    }

    void setSeed(size_t i) { gen_.seed(i); }

  private:
    static double get_exp_lambda_for_area(size_t interval, double area_under_interval) {
        return -std::log(1 - area_under_interval) / interval;
    }

    const size_t                    interval_;
    const double                    area_under_interval_;
    const lru_key_t                 max_key_;
    std::exponential_distribution<> dist_;
    std::mt19937                    gen_;
};

inline void test_rnd(size_t interval, double area_under_interval) {
    ExponentialKeyGenerator g(42, interval, area_under_interval,
                              std::numeric_limits<lru_key_t>::max());
    size_t                  reps   = interval * 1000;
    size_t                  hits   = 0;
    size_t                  hits_2 = 0;
    size_t                  hits_0 = 0;
    for (int i = 0; i < reps; i++) {
        auto x = g();
        if (x <= interval / 2)
            hits_2++;
        if (x <= interval)
            hits++;
        if (x == 0)
            hits_0++;
    }

    std::cout << "Test: interval=" << interval << ", lambda=" << g.dist_parameter()
              << ", p_exp=" << area_under_interval << ", p(X)_real=" << hits / double(reps)
              << ", p(X/2)_real=" << hits_2 / double(reps)
              << ", p(0)_real=" << hits_0 / double(reps) << std::endl;
}
