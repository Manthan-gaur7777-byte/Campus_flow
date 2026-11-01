#include "waitlist_heap.h"
#include <utility>

bool WaitlistHeap::better(const WaitEntry& a, const WaitEntry& b) {
    if (a.weight != b.weight) return a.weight > b.weight;
    if (a.regTime != b.regTime) return a.regTime < b.regTime;
    return a.id < b.id;
}

void WaitlistHeap::push(const WaitEntry& e) {
    h.push_back(e);
    siftUp((int)h.size() - 1);
}

void WaitlistHeap::siftUp(int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!better(h[i], h[p])) break;
        std::swap(h[i], h[p]);
        i = p;
    }
}

void WaitlistHeap::siftDown(int i) {
    int n = (int)h.size();
    while (true) {
        int l = 2 * i + 1, r = 2 * i + 2, best = i;
        if (l < n && better(h[l], h[best])) best = l;
        if (r < n && better(h[r], h[best])) best = r;
        if (best == i) break;
        std::swap(h[i], h[best]);
        i = best;
    }
}

WaitEntry WaitlistHeap::pop() {
    WaitEntry t = h[0];
    h[0] = h.back();
    h.pop_back();
    if (!h.empty()) siftDown(0);
    return t;
}

bool WaitlistHeap::removeById(const std::string& id) {
    for (int i = 0; i < (int)h.size(); i++) {
        if (h[i].id == id) {
            h[i] = h.back();
            h.pop_back();
            if (i < (int)h.size()) { siftUp(i); siftDown(i); }
            return true;
        }
    }
    return false;
}
