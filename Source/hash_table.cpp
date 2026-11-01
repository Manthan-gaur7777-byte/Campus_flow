#include "hash_table.h"
#include <algorithm>

HashTable::HashTable(int cap)
    : buckets_(static_cast<std::size_t>(std::max(cap, 1))) {}

std::size_t HashTable::hashFunction(const std::string& key) const {
    std::size_t h = 5381;                           // djb2
    for (unsigned char c : key) h = ((h << 5) + h) + c;
    return h % buckets_.size();
}

bool HashTable::insert(const Participant& p) {
    if (p.id.empty()) return false;
    if (find(p.id)) return false;                   // duplicate
    if (loadFactor() >= Config::MAX_LOAD) resizeTable();
    buckets_[hashFunction(p.id)].push_back(p);
    ++count_;
    return true;
}

Participant* HashTable::find(const std::string& id) {
    for (auto& p : buckets_[hashFunction(id)])
        if (p.id == id) return &p;
    return nullptr;
}

const Participant* HashTable::find(const std::string& id) const {
    for (const auto& p : buckets_[hashFunction(id)])
        if (p.id == id) return &p;
    return nullptr;
}

bool HashTable::erase(const std::string& id) {
    auto& b = buckets_[hashFunction(id)];
    for (auto it = b.begin(); it != b.end(); ++it)
        if (it->id == id) { b.erase(it); --count_; return true; }
    return false;
}

void HashTable::clear() {
    for (auto& b : buckets_) b.clear();
    count_ = 0;
}

void HashTable::resizeTable() {
    std::vector<std::list<Participant>> old;
    old.swap(buckets_);
    buckets_.assign(old.size() * 2 + 1, {});
    for (auto& b : old)
        for (auto& p : b)
            buckets_[hashFunction(p.id)].push_back(std::move(p));
}
