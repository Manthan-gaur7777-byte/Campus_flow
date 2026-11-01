#pragma once
#include <string>
#include <vector>

struct WaitEntry {
    std::string id;
    int weight;      // Participant::priority (higher = served first)
    long regTime;    // join order (smaller = joined earlier)
};

class WaitlistHeap {
public:
    void push(const WaitEntry& e);                    // O(log W)
    WaitEntry pop();                                  // O(log W), call only if !empty()
    const WaitEntry& top() const { return h[0]; }     // O(1), call only if !empty()
    bool removeById(const std::string& id);           // O(W)
    bool empty() const { return h.empty(); }
    int size() const { return (int)h.size(); }
    static bool better(const WaitEntry& a, const WaitEntry& b);   // a should be served before b
private:
    std::vector<WaitEntry> h;
    void siftUp(int i);
    void siftDown(int i);
};
