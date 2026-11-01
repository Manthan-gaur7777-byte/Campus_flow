#pragma once
#include "common.h"
#include <vector>
#include "segment_tree.h"

class SeatGrid {
public:
    SeatGrid(int rows, int cols);

    int getRows() const { return rows_; }
    int getCols() const { return cols_; }
    bool isValidPosition(int r, int c) const;

    // Zones are equal horizontal bands of rows (last zone takes any remainder).
    int zoneCount() const { return zones_; }
    int zoneOfRow(int r) const;
    void zoneRows(int zone, int& firstRow, int& lastRowExclusive) const;

    Seat* getSeat(int r, int c);
    const Seat* getSeat(int r, int c) const;

    bool allocateSeat(int r, int c, const std::string& pid);  // FREE -> ALLOCATED
    bool freeSeat(int r, int c);                              // ALLOCATED/OCCUPIED -> FREE
    bool occupySeat(int r, int c);                            // ALLOCATED -> OCCUPIED (check-in)
    bool blockSeat(int r, int c);                             // FREE -> BLOCKED
    bool reserveSeat(int r, int c);                           // FREE -> RESERVED

    int countFreeSeats() const;
    int countAllocatedSeats() const;
    int countOccupiedSeats() const;
    int countFreeInZone(int zone) const;
    int countUsedInZone(int zone) const;                      // ALLOCATED + OCCUPIED (for Member 3)

    // --- Segment-tree index (Member 2's structure), kept in sync automatically ---
    int flatIndex(int r, int c) const { return r * cols_ + c; }
    int findFirstFreeInZone(int zone) const;        // flat idx of first FREE seat, -1 if none
    int treeFreeInZone(int zone) const;             // O(log N) free count via the tree
    Counts zoneCounts(int zone) const;              // O(log N): free/reserved/allocated/occupied for a zone
    bool treeMatchesGrid() const;                   // brute-force cross-check (tests / Day 5)

    void reset();                                   // all seats FREE again (used by load)

    void display() const;

private:
    int rows_, cols_, zones_;
    std::vector<std::vector<Seat>> grid_;
    SegmentTree tree_;
    void sync(int r, int c);
    static TreeSeat toTree(SeatState s);
    int countState(SeatState s, int r0, int r1) const;
};
