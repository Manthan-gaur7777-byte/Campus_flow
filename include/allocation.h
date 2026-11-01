#pragma once
#include "hash_table.h"
#include "seat_grid.h"
#include <functional>

enum class AllocResult { OK, NOT_FOUND, ALREADY_SEATED, NO_SEATS, INVALID_SEAT, SEAT_TAKEN, NO_SEAT_ASSIGNED };

// The hash table and grid must always agree:
//   participant.seat == (r,c)  <=>  grid(r,c).participantId == participant.id
// Every method below keeps both sides updated together.
class AllocationManager {
public:
    AllocationManager(HashTable& table, SeatGrid& grid) : table_(table), grid_(grid) {}

    bool registerParticipant(const Participant& p);
    AllocResult allocateParticipant(const std::string& id);          // uses preferredZone
    AllocResult findAndAllocate(Participant& p, int zone);           // tries zone, then overflows
    AllocResult cancelParticipant(const std::string& id);            // frees seat if any
    AllocResult reassignParticipant(const std::string& id, int r, int c);
    AllocResult checkInSeat(const std::string& id);                  // ALLOCATED -> OCCUPIED
    AllocResult releaseSeat(const std::string& id);                  // no-show: free seat, keep record

    bool isConsistent() const;                                        // full cross-check, for tests/Day 5

    // Seat freed callback hook for Member 2 (called with the freed row/col)
    using FreedHook = std::function<void(int row, int col)>;
    void setSeatFreedHook(FreedHook h) { onFreed_ = h; }

private:
    HashTable& table_;
    SeatGrid& grid_;
    FreedHook onFreed_ = nullptr;

    bool tryZone(Participant& p, int zone);
    void notifyFreed(int r, int c) { if (onFreed_) onFreed_(r, c); }
};
