#pragma once
#include <vector>

enum TreeSeat { FREE_GENERAL, FREE_RESERVED, ALLOCATED, OCCUPIED, BLOCKED };
enum FreeKind { GENERAL, RESERVED };

struct Counts {
    int freeGeneral = 0, freeReserved = 0, allocated = 0, occupied = 0;
};

Counts merge(const Counts& a, const Counts& b);

class SegmentTree {
public:
    void build(const std::vector<TreeSeat>& seats);
    void update(int idx, TreeSeat s);      // O(log N)
    Counts query(int l, int r) const;       // inclusive range, O(log N)
    int size() const { return n; }
    int findFirstFree(int l, int r, FreeKind kind) const;   // -1 if none
    
private:
    int n = 0;
    std::vector<Counts> tree;               // 4*n nodes, node 1 = root
    void buildRec(int node, int l, int r, const std::vector<TreeSeat>& seats);
    void updateRec(int node, int l, int r, int idx, TreeSeat s);
    Counts queryRec(int node, int l, int r, int ql, int qr) const;
    int findRec(int node, int l, int r, int ql, int qr, FreeKind kind) const;
};