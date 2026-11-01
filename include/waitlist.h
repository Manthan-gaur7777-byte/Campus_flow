#pragma once
#include "allocation.h"
#include "waitlist_heap.h"
#include <string>
#include <unordered_set>
#include <vector>

enum class SeatOutcome { SEATED, WAITLISTED, ALREADY_SEATED, ALREADY_WAITING, NOT_FOUND };

struct Reassignment { std::string id; int row; int col; };

// Member 2: waitlist (one heap per zone), seat-freed reassignment, simulated clock + no-show run.
// Check-in itself is Member 1's AllocationManager::checkInSeat (not duplicated here).
class WaitlistManager {
public:
    WaitlistManager(HashTable& table, SeatGrid& grid, AllocationManager& mgr, int graceMinutes = 15);

    SeatOutcome requestSeat(const std::string& id);   // seat now, or join the waitlist if venue is full
    AllocResult cancel(const std::string& id);        // leaves waitlist and/or frees seat

    void advanceClock(int minutes) { if (minutes > 0) now_ += minutes; }
    int  clock() const { return now_; }
    int  grace() const { return grace_; }
    bool graceOver() const { return now_ >= grace_; }
    int  runNoShow();                                 // -1 if grace not over, else number released

    int  waitingTotal() const { return (int)waiting_.size(); }
    int  waitingInZone(int zone) const;
    bool isWaiting(const std::string& id) const { return waiting_.count(id) > 0; }
    void display() const;
    std::vector<Reassignment> takeReassignments();    // returns and clears the log

private:
    HashTable& table_;
    SeatGrid& grid_;
    AllocationManager& mgr_;
    std::vector<WaitlistHeap> heaps_;                 // one per zone
    std::unordered_set<std::string> waiting_;         // O(1) "is waiting?" bookkeeping
    std::vector<Reassignment> log_;
    long seq_ = 0;                                    // join order counter (tie-break)
    int now_ = 0, grace_;

    void onSeatFreed(int r, int c);
    int bestZone(int preferred) const;                // -1 if all heaps empty
};
