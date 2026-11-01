#pragma once
#include "seat_grid.h"

enum class OccLevel { LOW, NORMAL, HIGH, OVERCROWDED };

// Configurable thresholds (fractions of capacity). Defaults: <30% low, <70% normal, <90% high, else overcrowded.
struct OccThresholds {
    double normalFrom = 0.30;
    double highFrom   = 0.70;
    double overFrom   = 0.90;
};

struct ZoneOccupancy {
    int zone = -1;
    int capacity = 0;     // seats that can ever be used (everything except BLOCKED)
    int checkedIn = 0;    // OCCUPIED
    int booked = 0;       // ALLOCATED, not arrived yet
    int free = 0;         // free general seats (allocatable)
    int reserved = 0;     // free reserved seats
    double pct = 0.0;     // checkedIn / capacity (0 when capacity is 0)
    OccLevel level = OccLevel::LOW;
};

const char* levelName(OccLevel l);

// Member 2: reads all numbers from the segment tree (O(log N) per zone), never from counters.
class OccupancyMonitor {
public:
    explicit OccupancyMonitor(const SeatGrid& grid, OccThresholds t = OccThresholds()) : grid_(grid), th_(t) {}

    void setThresholds(const OccThresholds& t) { th_ = t; }
    OccLevel classify(double pct) const;
    ZoneOccupancy zone(int z) const;          // O(log N)
    ZoneOccupancy venue() const;              // O(Z log N)
    void display(int waitingCount) const;     // terminal report
private:
    const SeatGrid& grid_;
    OccThresholds th_;
    ZoneOccupancy fromCounts(int zone, const Counts& k) const;
};
