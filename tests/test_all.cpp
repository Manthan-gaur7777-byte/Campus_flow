#include "persistence.h"
#include <fstream>
#include <cassert>
#include <iostream>
#include <cstdlib>

static int hookCalls = 0;
static void hook(int, int) { ++hookCalls; }

static void testHash() {
    HashTable t(3);
    assert(t.insert(makeParticipant("A1", "x")));
    assert(!t.insert(makeParticipant("A1", "dup")));   // duplicate
    assert(!t.insert(makeParticipant("", "empty")));   // invalid ID
    for (int i = 0; i < 500; ++i) assert(t.insert(makeParticipant("P" + std::to_string(i), "n")));
    assert(t.size() == 501);
    assert(t.loadFactor() <= Config::MAX_LOAD + 1e-9); // resize kept load low
    for (int i = 0; i < 500; ++i) assert(t.find("P" + std::to_string(i)));
    assert(t.erase("P7") && !t.find("P7") && !t.erase("P7"));
    HashTable copy = t;                                 // safe deep copy
    t.clear();
    assert(t.size() == 0 && copy.size() == 500 && copy.find("P8"));
    assert(t.insert(makeParticipant("again", "ok")));   // reusable after clear
    std::cout << "hash ok\n";
}

static void testGrid() {
    SeatGrid g(6, 8);
    assert(g.countFreeSeats() == 48 && g.zoneCount() == 3);
    assert(g.zoneOfRow(0) == 0 && g.zoneOfRow(2) == 1 && g.zoneOfRow(5) == 2);
    assert(g.allocateSeat(0, 0, "P1") && !g.allocateSeat(0, 0, "P2"));
    assert(g.blockSeat(0, 1) && g.reserveSeat(1, 1));
    assert(!g.blockSeat(0, 0));                          // can't block an allocated seat
    assert(!g.allocateSeat(0, 1, "P3") && !g.allocateSeat(1, 1, "P3"));
    assert(!g.allocateSeat(9, 9, "P3") && !g.allocateSeat(-1, 0, "P3") && !g.allocateSeat(1, 2, ""));
    assert(g.countFreeSeats() == 45 && g.countAllocatedSeats() == 1);
    assert(g.freeSeat(0, 0) && !g.freeSeat(0, 0) && !g.freeSeat(0, 1)); // blocked seat can't be "freed"
    std::cout << "grid ok\n";
}

static void testAllocation() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g);
    m.setSeatFreedHook(hook);
    assert(m.registerParticipant(makeParticipant("P1", "a", 1, 0)));
    assert(!m.registerParticipant(makeParticipant("P1", "a")));
    assert(m.allocateParticipant("P1") == AllocResult::OK);
    assert(m.allocateParticipant("P1") == AllocResult::ALREADY_SEATED);
    assert(m.allocateParticipant("nobody") == AllocResult::NOT_FOUND);
    assert(t.find("P1")->seatRow == 0 && t.find("P1")->seatCol == 0);

    // reserved + blocked seats are skipped
    g.reserveSeat(0, 1); g.blockSeat(0, 2);
    m.registerParticipant(makeParticipant("P2", "b", 1, 0));
    m.allocateParticipant("P2");
    assert(t.find("P2")->seatCol == 3);

    // reassign: failure leaves old seat untouched, success frees the old one
    assert(m.reassignParticipant("P2", 9, 9) == AllocResult::INVALID_SEAT);
    assert(m.reassignParticipant("P2", 0, 0) == AllocResult::SEAT_TAKEN);
    assert(t.find("P2")->seatCol == 3 && m.isConsistent());
    assert(m.reassignParticipant("P2", 5, 7) == AllocResult::OK);
    assert(g.getSeat(0, 3)->state == SeatState::FREE && m.isConsistent());

    // cancel frees seat + fires hook
    int before = hookCalls;
    assert(m.cancelParticipant("P1") == AllocResult::OK);
    assert(hookCalls == before + 1 && g.getSeat(0, 0)->state == SeatState::FREE && m.isConsistent());

    // check-in then no-show path
    assert(m.checkInSeat("P2") == AllocResult::OK && g.getSeat(5, 7)->state == SeatState::OCCUPIED);
    assert(m.releaseSeat("P2") == AllocResult::OK && !t.find("P2")->hasSeat() && m.isConsistent());
    assert(m.releaseSeat("P2") == AllocResult::NO_SEAT_ASSIGNED);
    std::cout << "allocation ok\n";
}

static void testZoneOverflowAndFull() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g);
    // 20 people all prefer zone 0 (16 seats): the last 4 overflow into zone 1
    for (int i = 0; i < 20; ++i) {
        m.registerParticipant(makeParticipant("Z" + std::to_string(i), "n", 0, 0));
        assert(m.allocateParticipant("Z" + std::to_string(i)) == AllocResult::OK);
    }
    assert(g.countFreeInZone(0) == 0 && g.countUsedInZone(1) == 4 && m.isConsistent());
    // fill the rest, then one more must fail cleanly
    for (int i = 20; i < 48; ++i) {
        m.registerParticipant(makeParticipant("Z" + std::to_string(i), "n", 0, 2));
        assert(m.allocateParticipant("Z" + std::to_string(i)) == AllocResult::OK);
    }
    m.registerParticipant(makeParticipant("late", "n"));
    assert(m.allocateParticipant("late") == AllocResult::NO_SEATS);
    assert(!t.find("late")->hasSeat() && m.isConsistent());
    std::cout << "zone overflow ok\n";
}

static void testTreeSync() {
    // random operations: tree must always equal brute force, zone counts must match
    SeatGrid g(6, 8); srand(7);
    for (int i = 0; i < 4000; ++i) {
        int r = rand() % 6, c = rand() % 8;
        switch (rand() % 5) {
            case 0: g.allocateSeat(r, c, "x"); break;
            case 1: g.freeSeat(r, c); break;
            case 2: g.blockSeat(r, c); break;
            case 3: g.reserveSeat(r, c); break;
            case 4: g.occupySeat(r, c); break;
        }
        assert(g.treeMatchesGrid());
    }
    for (int z = 0; z < g.zoneCount(); ++z) {
        int a, b; g.zoneRows(z, a, b);
        int brute = 0, first = -1;
        for (int r = a; r < b; ++r) for (int c = 0; c < 8; ++c)
            if (g.getSeat(r, c)->state == SeatState::FREE) { ++brute; if (first < 0) first = r * 8 + c; }
        assert(g.countFreeInZone(z) == brute && g.findFirstFreeInZone(z) == first);
    }
    std::cout << "tree sync ok\n";
}

static void testPersistence() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g);
    assert(loadSampleData(m, 30) == 30);
    assert(loadSampleData(m, 30) == 0);                 // reloading skips duplicates
    g.blockSeat(5, 7); g.reserveSeat(5, 6);
    for (int i = 0; i < 20; ++i) m.allocateParticipant("P" + std::to_string(101 + i));
    m.checkInSeat("P101"); m.releaseSeat("P102"); m.cancelParticipant("P103");
    m.registerParticipant(makeParticipant("X1", "Name|with|bars"));
    assert(saveToFile(t, g, "cf_test.txt"));

    HashTable t2; SeatGrid g2(6, 8); AllocationManager m2(t2, g2);
    assert(loadFromFile(t2, g2, "cf_test.txt"));
    assert(t2.size() == t.size() && m2.isConsistent());
    assert(g2.countFreeSeats() == g.countFreeSeats());
    assert(g2.countOccupiedSeats() == g.countOccupiedSeats());
    assert(g2.getSeat(5, 7)->state == SeatState::BLOCKED && g2.getSeat(5, 6)->state == SeatState::RESERVED);
    assert(t2.find("P101")->state == ParticipantState::CHECKED_IN);
    assert(t2.find("P102")->state == ParticipantState::NO_SHOW && !t2.find("P102")->hasSeat());
    assert(t2.find("X1")->name == "Name with bars");

    // corrupt / wrong-size / missing files are rejected and leave current data untouched
    { std::ofstream f("cf_bad.txt"); f << "CAMPUSFLOW 1 6 8\nP|A|b|x|0|0|0|0\n"; }
    size_t before = t2.size();
    assert(!loadFromFile(t2, g2, "cf_bad.txt") && t2.size() == before && m2.isConsistent());
    { std::ofstream f("cf_dup.txt"); f << "CAMPUSFLOW 1 6 8\nP|A|a|0|0|0|0|1\nP|B|b|0|0|0|0|1\n"; }  // same seat twice
    assert(!loadFromFile(t2, g2, "cf_dup.txt"));
    { std::ofstream f("cf_size.txt"); f << "CAMPUSFLOW 1 3 3\n"; }
    assert(!loadFromFile(t2, g2, "cf_size.txt"));
    assert(!loadFromFile(t2, g2, "does_not_exist.txt"));
    std::remove("cf_test.txt"); std::remove("cf_bad.txt"); std::remove("cf_dup.txt"); std::remove("cf_size.txt");
    std::cout << "persistence ok\n";
}

static void testEdgeCases() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g);
    assert(m.reassignParticipant("ghost", 0, 0) == AllocResult::NOT_FOUND);
    assert(m.cancelParticipant("ghost") == AllocResult::NOT_FOUND);
    m.registerParticipant(makeParticipant("A", "a"));
    assert(m.reassignParticipant("A", 0, 0) == AllocResult::NO_SEAT_ASSIGNED);   // not seated yet
    assert(m.checkInSeat("A") == AllocResult::NO_SEAT_ASSIGNED);
    assert(m.cancelParticipant("A") == AllocResult::OK);                          // cancel w/o seat
    assert(m.allocateParticipant("A") == AllocResult::OK);                        // re-allocate after cancel
    assert(m.checkInSeat("A") == AllocResult::OK && m.checkInSeat("A") == AllocResult::OK);  // idempotent
    // bad zone numbers fall back safely
    Participant p = makeParticipant("B", "b"); m.registerParticipant(p);
    Participant* pb = t.find("B");
    assert(m.findAndAllocate(*pb, 99) == AllocResult::OK && m.isConsistent());
    // tiny / empty grids do not crash
    SeatGrid tiny(1, 1), empty(0, 0);
    HashTable t3; AllocationManager m3(t3, tiny);
    m3.registerParticipant(makeParticipant("Q", "q"));
    assert(m3.allocateParticipant("Q") == AllocResult::OK);
    m3.registerParticipant(makeParticipant("R", "r"));
    assert(m3.allocateParticipant("R") == AllocResult::NO_SEATS);
    assert(empty.findFirstFreeInZone(0) == -1 && empty.countFreeSeats() == 0);
    std::cout << "edge cases ok\n";
}

int main() {
    testPersistence(); testEdgeCases();
    testTreeSync();
    testHash(); testGrid(); testAllocation(); testZoneOverflowAndFull();
    std::cout << "ALL TESTS PASSED\n";
}
