#pragma once

#include <memory>
#include "common.h"

class RandomBenchmarkApp;

class KeyGenerator : public std::enable_shared_from_this<KeyGenerator> {
  public:
    using ptr_t = std::shared_ptr<KeyGenerator>;

    static ptr_t factory(RandomBenchmarkApp& b, const std::string& name, lru_key_t max_key);

    virtual ~KeyGenerator() = default;

    virtual std::string name() const = 0;
    virtual ptr_t clone() const = 0;
    virtual void setThread(size_t id, size_t count) = 0;

    virtual lru_key_t getKey() = 0;
};

