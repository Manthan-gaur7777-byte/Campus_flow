#include "waitlist.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

static void fill(HashTable& t, WaitlistManager& w, int n) {
    for (int i = 0; i < n; ++i) {
        std::string id = "S" + std::to_string(i);
        t.insert(makeParticipant(id, "n", 0, 0));
        assert(w.requestSeat(id) == SeatOutcome::SEATED);
    }
}

static void testPriorityAndZones() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g); WaitlistManager w(t, g, m);
    fill(t, w, 48);
    t.insert(makeParticipant("X1", "n", 1, 0)); t.insert(makeParticipant("X2", "n", 5, 0)); t.insert(makeParticipant("X3", "n", 5, 1));
    assert(w.requestSeat("X1") == SeatOutcome::WAITLISTED);
    assert(w.requestSeat("X2") == SeatOutcome::WAITLISTED);
    assert(w.requestSeat("X3") == SeatOutcome::WAITLISTED);
    assert(w.requestSeat("X1") == SeatOutcome::ALREADY_WAITING);
    assert(w.requestSeat("S0") == SeatOutcome::ALREADY_SEATED);
    assert(w.requestSeat("ghost") == SeatOutcome::NOT_FOUND);
    assert(w.waitingTotal() == 3 && w.waitingInZone(0) == 2 && w.waitingInZone(1) == 1);

    // seat freed in zone 0 -> best zone-0 waiter (X2, priority 5 beats X1)
    int zone0 = g.zoneOfRow(t.find("S0")->seatRow);
    assert(zone0 == 0 && w.cancel("S0") == AllocResult::OK);
    assert(t.find("X2")->hasSeat() && !t.find("X1")->hasSeat() && g.zoneOfRow(t.find("X2")->seatRow) == 0);

    // seat freed in zone 2 (nobody waiting there) -> best top elsewhere: X3 (5) beats X1 (1)
    assert(t.find("S47")->seatRow >= 4 && w.cancel("S47") == AllocResult::OK);
    assert(t.find("X3")->hasSeat() && !t.find("X1")->hasSeat());
    assert(w.waitingTotal() == 1 && m.isConsistent());

    // cancelling a waiting person removes them; next freed seat just stays free
    assert(w.cancel("X1") == AllocResult::OK && w.waitingTotal() == 0);
    assert(w.cancel("S1") == AllocResult::OK && g.countFreeSeats() == 1 && m.isConsistent());
    assert(w.cancel("ghost") == AllocResult::NOT_FOUND);
    std::cout << "priority + zones ok\n";
}

static void testNoShow() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g); WaitlistManager w(t, g, m, 15);
    fill(t, w, 48);
    for (int i = 0; i < 48; ++i) if (i != 10 && i != 20) assert(m.checkInSeat("S" + std::to_string(i)) == AllocResult::OK);
    t.insert(makeParticipant("W1", "n", 3, 0)); t.insert(makeParticipant("W2", "n", 1, 0));
    w.requestSeat("W2"); w.requestSeat("W1");
    assert(w.runNoShow() == -1);                       // grace period not over yet
    w.advanceClock(15);
    assert(w.runNoShow() == 2);
    assert(t.find("S10")->state == ParticipantState::NO_SHOW && t.find("S20")->state == ParticipantState::NO_SHOW);
    assert(t.find("W1")->state == ParticipantState::ALLOCATED && t.find("W2")->state == ParticipantState::ALLOCATED);
    assert(w.waitingTotal() == 0 && m.isConsistent());
    auto log = w.takeReassignments();
    assert(log.size() == 2 && log[0].id == "W1");      // higher priority was seated first
    assert(w.takeReassignments().empty());
    assert(w.runNoShow() == 2);                        // W1/W2 now count as no-shows too (never checked in)
    std::cout << "no-show ok\n";

    // empty waitlist: released seats simply become FREE
    HashTable t2; SeatGrid g2(6, 8); AllocationManager m2(t2, g2); WaitlistManager w2(t2, g2, m2, 0);
    fill(t2, w2, 10);
    assert(w2.runNoShow() == 10 && g2.countFreeSeats() == 48 && m2.isConsistent());
    assert(w2.runNoShow() == 0);                       // second run changes nothing
    std::cout << "no-show empty waitlist ok\n";
}

static void testRandom() {
    HashTable t; SeatGrid g(6, 8); AllocationManager m(t, g); WaitlistManager w(t, g, m, 5);
    srand(11); int next = 0;
    for (int it = 0; it < 5000; ++it) {
        int op = rand() % 6;
        if (op <= 1 || next == 0) {
            std::string id = "R" + std::to_string(next++);
            t.insert(makeParticipant(id, "n", rand() % 10, rand() % 3));
            w.requestSeat(id);
        } else {
            std::string id = "R" + std::to_string(rand() % next);
            if (op == 2) w.cancel(id);
            else if (op == 3) m.checkInSeat(id);
            else if (op == 4) m.reassignParticipant(id, rand() % 6, rand() % 8);
            else { w.advanceClock(1); w.runNoShow(); }
        }
        assert(m.isConsistent());
        if (w.waitingTotal() > 0) assert(g.countFreeSeats() == 0);   // waiters exist only while venue is full
    }
    std::cout << "random ok (" << next << " people)\n";
}

int main() { testPriorityAndZones(); testNoShow(); testRandom(); std::cout << "WAITLIST TESTS PASSED\n"; }
