#include "occupancy.h"
#include "waitlist.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

static void testClassify() {
    SeatGrid g(6, 8); OccupancyMonitor mon(g);
    assert(mon.classify(0.0) == OccLevel::LOW && mon.classify(0.2999) == OccLevel::LOW);
    assert(mon.classify(0.30) == OccLevel::NORMAL && mon.classify(0.6999) == OccLevel::NORMAL);
    assert(mon.classify(0.70) == OccLevel::HIGH && mon.classify(0.8999) == OccLevel::HIGH);
    assert(mon.classify(0.90) == OccLevel::OVERCROWDED && mon.classify(1.0) == OccLevel::OVERCROWDED);
    OccThresholds t; t.normalFrom = 0.5; t.highFrom = 0.8; t.overFrom = 0.95;   // configurable
    mon.setThresholds(t);
    assert(mon.classify(0.40) == OccLevel::LOW && mon.classify(0.85) == OccLevel::HIGH);
    std::cout << "classify ok\n";
}

static void testZoneLevels() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g); WaitlistManager w(t, g, m);
    OccupancyMonitor mon(g);
    ZoneOccupancy e = mon.zone(0);
    assert(e.capacity == 16 && e.checkedIn == 0 && e.free == 16 && e.level == OccLevel::LOW);
    for (int i = 0; i < 16; ++i) {                     // 16 people fill zone 0 exactly
        std::string id = "Z" + std::to_string(i);
        t.insert(makeParticipant(id, "n", 0, 0)); assert(w.requestSeat(id) == SeatOutcome::SEATED);
    }
    assert(mon.zone(0).booked == 16 && mon.zone(0).free == 0 && mon.zone(0).pct == 0.0);   // booked != checked in
    int expectCheck[] = {4, 5, 11, 12, 14, 15};
    OccLevel expectLevel[] = {OccLevel::LOW, OccLevel::NORMAL, OccLevel::NORMAL, OccLevel::HIGH, OccLevel::HIGH, OccLevel::OVERCROWDED};
    int done = 0;
    for (int k = 0; k < 6; ++k) {
        while (done < expectCheck[k]) assert(m.checkInSeat("Z" + std::to_string(done++)) == AllocResult::OK);
        ZoneOccupancy z = mon.zone(0);
        assert(z.checkedIn == expectCheck[k] && z.level == expectLevel[k]);
    }
    assert(mon.zone(0).checkedIn == 15 && mon.zone(0).booked == 1);
    assert(mon.zone(1).checkedIn == 0 && mon.zone(1).level == OccLevel::LOW);   // other zones untouched
    assert(mon.venue().checkedIn == 15 && mon.venue().capacity == 48);
    std::cout << "zone levels ok\n";
}

static void testEdges() {
    SeatGrid g(6, 8); OccupancyMonitor mon(g);
    for (int c = 0; c < 8; ++c) { g.blockSeat(0, c); g.blockSeat(1, c); }     // zone 0 fully blocked
    ZoneOccupancy z = mon.zone(0);
    assert(z.capacity == 0 && z.pct == 0.0 && z.level == OccLevel::LOW);       // no division by zero
    g.reserveSeat(2, 0);
    assert(mon.zone(1).reserved == 1 && mon.zone(1).capacity == 16);
    assert(mon.zone(-1).capacity == 0 && mon.zone(99).capacity == 0);           // bad zone numbers are safe
    SeatGrid empty(0, 0); OccupancyMonitor me(empty);
    assert(me.venue().capacity == 0);
    std::cout << "edges ok\n";
}

static void testLastSeatRace() {
    // two requests for the last free seat: exactly one is seated, the other waitlisted
    HashTable t; SeatGrid g(1, 1); AllocationManager m(t, g); WaitlistManager w(t, g, m);
    t.insert(makeParticipant("A", "a")); t.insert(makeParticipant("B", "b"));
    SeatOutcome a = w.requestSeat("A"), b = w.requestSeat("B");
    assert(a == SeatOutcome::SEATED && b == SeatOutcome::WAITLISTED && m.isConsistent());
    std::cout << "last seat ok\n";
}

static void testAgainstBruteForce() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g); WaitlistManager w(t, g, m, 3);
    OccupancyMonitor mon(g);
    srand(5); int next = 0;
    for (int it = 0; it < 4000; ++it) {
        int op = rand() % 8;
        if (op <= 1 || next == 0) {
            std::string id = "B" + std::to_string(next++);
            t.insert(makeParticipant(id, "n", rand() % 10, rand() % 3)); w.requestSeat(id);
        } else {
            std::string id = "B" + std::to_string(rand() % next);
            if (op == 2) w.cancel(id);
            else if (op == 3 || op == 4) m.checkInSeat(id);
            else if (op == 5) g.blockSeat(rand() % 6, rand() % 8);
            else if (op == 6) g.reserveSeat(rand() % 6, rand() % 8);
            else { w.advanceClock(1); w.runNoShow(); }
        }
        for (int z = 0; z < 3; ++z) {                   // tree numbers must equal a plain scan of the grid
            int a, b; g.zoneRows(z, a, b);
            int cap = 0, occ = 0, al = 0, fr = 0, rs = 0;
            for (int r = a; r < b; ++r) for (int c = 0; c < 8; ++c) {
                SeatState s = g.getSeat(r, c)->state;
                if (s != SeatState::BLOCKED) ++cap;
                if (s == SeatState::OCCUPIED) ++occ;
                if (s == SeatState::ALLOCATED) ++al;
                if (s == SeatState::FREE) ++fr;
                if (s == SeatState::RESERVED) ++rs;
            }
            ZoneOccupancy o = mon.zone(z);
            assert(o.capacity == cap && o.checkedIn == occ && o.booked == al && o.free == fr && o.reserved == rs);
        }
    }
    std::cout << "brute force ok\n";
}

int main() { testClassify(); testZoneLevels(); testEdges(); testLastSeatRace(); testAgainstBruteForce();
             std::cout << "OCCUPANCY TESTS PASSED\n"; }
