#pragma once
#include "common.h"
#include <vector>
#include <list>
#include <cstddef>

// Separate-chaining hash table keyed by participant ID.
// Uses std::list buckets (no raw pointers) => no leaks, safe copy/move.
class HashTable {
public:
    explicit HashTable(int initialCapacity = Config::INITIAL_BUCKETS);

    bool insert(const Participant& p);              // false: empty ID or duplicate
    Participant* find(const std::string& id);       // nullptr if absent
    const Participant* find(const std::string& id) const;
    bool erase(const std::string& id);
    void clear();

    std::size_t size() const { return count_; }
    std::size_t capacity() const { return buckets_.size(); }
    double loadFactor() const { return double(count_) / double(buckets_.size()); }

    template <typename F> void forEach(F fn) {      // iterate all participants
        for (auto& b : buckets_) for (auto& p : b) fn(p);
    }

private:
    std::vector<std::list<Participant>> buckets_;
    std::size_t count_ = 0;

    std::size_t hashFunction(const std::string& key) const;
    void resizeTable();                             // doubles capacity, rehashes
};
