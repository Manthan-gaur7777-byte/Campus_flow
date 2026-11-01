#include "segment_tree.h"

Counts merge(const Counts& a, const Counts& b) {
    Counts c;
    c.freeGeneral  = a.freeGeneral  + b.freeGeneral;
    c.freeReserved = a.freeReserved + b.freeReserved;
    c.allocated    = a.allocated    + b.allocated;
    c.occupied     = a.occupied     + b.occupied;
    return c;
}

static Counts leafCounts(TreeSeat s) {
    Counts c;
    if (s == FREE_GENERAL)  c.freeGeneral = 1;
    if (s == FREE_RESERVED) c.freeReserved = 1;
    if (s == ALLOCATED)     c.allocated = 1;
    if (s == OCCUPIED)      c.occupied = 1;
    return c;                               // BLOCKED counts as nothing
}

void SegmentTree::build(const std::vector<TreeSeat>& seats) {
    n = (int)seats.size();
    tree.assign(4 * n + 4, Counts());
    if (n > 0) buildRec(1, 0, n - 1, seats);
}

void SegmentTree::buildRec(int node, int l, int r, const std::vector<TreeSeat>& seats) {
    if (l == r) { tree[node] = leafCounts(seats[l]); return; }
    int m = (l + r) / 2;
    buildRec(2 * node, l, m, seats);
    buildRec(2 * node + 1, m + 1, r, seats);
    tree[node] = merge(tree[2 * node], tree[2 * node + 1]);
}

void SegmentTree::update(int idx, TreeSeat s) {
    if (idx < 0 || idx >= n) return;
    updateRec(1, 0, n - 1, idx, s);
}

void SegmentTree::updateRec(int node, int l, int r, int idx, TreeSeat s) {
    if (l == r) { tree[node] = leafCounts(s); return; }
    int m = (l + r) / 2;
    if (idx <= m) updateRec(2 * node, l, m, idx, s);
    else          updateRec(2 * node + 1, m + 1, r, idx, s);
    tree[node] = merge(tree[2 * node], tree[2 * node + 1]);
}

Counts SegmentTree::query(int l, int r) const {
    if (n == 0 || l < 0 || r >= n || l > r) return Counts();
    return queryRec(1, 0, n - 1, l, r);
}

Counts SegmentTree::queryRec(int node, int l, int r, int ql, int qr) const {
    if (qr < l || r < ql) return Counts();          // no overlap
    if (ql <= l && r <= qr) return tree[node];      // fully inside
    int m = (l + r) / 2;
    return merge(queryRec(2 * node, l, m, ql, qr),
                 queryRec(2 * node + 1, m + 1, r, ql, qr));
}
static int freeOf(const Counts& c, FreeKind k) {
    return (k == GENERAL) ? c.freeGeneral : c.freeReserved;
}

int SegmentTree::findFirstFree(int l, int r, FreeKind kind) const {
    if (n == 0 || l < 0 || r >= n || l > r) return -1;
    return findRec(1, 0, n - 1, l, r, kind);
}

int SegmentTree::findRec(int node, int l, int r, int ql, int qr, FreeKind kind) const {
    if (qr < l || r < ql) return -1;                 // no overlap
    if (freeOf(tree[node], kind) == 0) return -1;    // nothing free below
    if (l == r) return l;                            // found a leaf
    int m = (l + r) / 2;
    int res = findRec(2 * node, l, m, ql, qr, kind);
    if (res != -1) return res;
    return findRec(2 * node + 1, m + 1, r, ql, qr, kind);
}