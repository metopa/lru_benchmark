#include "key_generator.h"

#include <random>

#include "benchmark.h"

class NormalGenerator final : public KeyGenerator {
  public:
    NormalGenerator(lru_key_t max_key)
            : max_key(max_key), gen(42), dist(max_key / 2., 0.315 * max_key / 2) {}

    std::string name() const override { return "normal"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new NormalGenerator(*this)); }

    void setThread(size_t id, size_t count) override { gen.seed(id); }

    KeySequence getKey() override {
        return {std::min(static_cast<lru_key_t>(std::abs(dist(gen))), max_key), 1};
    }

    uint64_t getUniqueCount() const override {
        return max_key;
    }

    lru_key_t max_key;
    std::mt19937 gen;
    std::normal_distribution<double> dist;
};

class UniformGenerator final : public KeyGenerator {
  public:
    UniformGenerator(lru_key_t max_key) : unique_count(max_key), gen(42), dist(0, max_key - 1) {}

    std::string name() const override { return "uniform"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new UniformGenerator(*this)); }

    void setThread(size_t id, size_t count) override { gen.seed(id); }

    KeySequence getKey() override { return {dist(gen), 1}; }

    uint64_t getUniqueCount() const override {
        return unique_count;
    }

    lru_key_t unique_count;
    std::mt19937 gen;
    std::uniform_int_distribution<lru_key_t> dist;
};

class ExpGenerator final : public KeyGenerator {
  public:
    ExpGenerator(size_t capacity, float alpha) : unique_count(capacity * alpha), gen(42),
                                                 dist(getLambda(capacity, alpha)) {}

    std::string name() const override { return "exp"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new ExpGenerator(*this)); }

    void setThread(size_t id, size_t count) override { gen.seed(id); }

    KeySequence getKey() override { return {static_cast<lru_key_t>(dist(gen)), 1}; }

    static double getLambda(size_t interval, double area_under_interval) {
        return -std::log(1 - area_under_interval) / interval;
    }

    uint64_t getUniqueCount() const override {
        return unique_count;
    }

    lru_key_t unique_count;
    std::mt19937 gen;
    std::exponential_distribution<double> dist;
};

class SameGenerator final : public KeyGenerator {
  public:
    SameGenerator() : state_(0) {}

    SameGenerator(size_t state) : state_(state) {}

    std::string name() const override { return "same"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new SameGenerator(*this)); }

    void setThread(size_t thread, size_t count) override {}

    KeySequence getKey() override { return {state_++, 1}; }

    size_t state_;
};

class VarSameGenerator final : public KeyGenerator {
  public:
    VarSameGenerator() : state_(0), gen(42), dist(0, 40) {}

    std::string name() const override { return "varsame"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new VarSameGenerator(*this)); }

    void setThread(size_t id, size_t count) override { gen.seed(id); }

    KeySequence getKey() override { return {++state_ + dist(gen), 1}; }

    std::mt19937 gen;
    std::uniform_int_distribution<lru_key_t> dist;
    size_t state_;
};

class DisjointGenerator final : public KeyGenerator {
  public:
    DisjointGenerator() : state_(0) {}

    DisjointGenerator(size_t state) : state_(state) {}

    std::string name() const override { return "disjoint"; }

    ptr_t clone() const override { return KeyGenerator::ptr_t(new DisjointGenerator(*this)); }

    void setThread(size_t thread, size_t count) override { state_ = thread * (1 << 30); }

    KeySequence getKey() override { return {state_++, 1}; }

    size_t state_;
};

class TraceGenerator final : public KeyGenerator {
    std::string trace_name_;
    std::shared_ptr<const Trace> trace_;
    size_t thread_id_;
    size_t thread_count_;
    size_t current_index_;

  public:
    TraceGenerator(const std::string& traceName) :
            trace_name_(traceName), trace_(std::make_shared<const Trace>(readTrace(trace_name_))),
            thread_id_(0), thread_count_(1), current_index_(0) {}


    std::string name() const override {
        return "trace:" + trace_name_;
    }

    ptr_t clone() const override {
        return std::make_shared<TraceGenerator>(*this);
    }

    void setThread(size_t id, size_t count) override {
        thread_id_ = id;
        thread_count_ = count;
        current_index_ = id;
    }

    KeySequence getKey() override {
        if (current_index_ >= trace_->data.size()) {
            current_index_ = thread_id_;
        }
        KeySequence res = trace_->data[current_index_];
        current_index_ += thread_count_;
        return res;
    }

    uint64_t getUniqueCount() const override {
        return trace_->distinct_count;
    }
};

KeyGenerator::ptr_t KeyGenerator::factory(RandomBenchmarkApp& b, const std::string& name,
                                          lru_key_t max_key) {
    if (name == "normal") {
        return ptr_t(new NormalGenerator(max_key));
    }
    if (name == "uniform") {
        return ptr_t(new UniformGenerator(max_key));
    }
    if (name == "same") {
        return ptr_t(new SameGenerator());
    }
    if (name == "varsame") {
        return ptr_t(new VarSameGenerator());
    }
    if (name == "disjoint") {
        return ptr_t(new DisjointGenerator());
    }
    if (name == "exp") {
        return ptr_t(new ExpGenerator(b.capacity, 0.8));
    }
    if (name.substr(0, 7) == "traces/") {
        return ptr_t(new TraceGenerator(name));
    }
    throw std::runtime_error("Unknown generator: " + name);
}
