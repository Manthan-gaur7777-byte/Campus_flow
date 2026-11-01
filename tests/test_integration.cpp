#include "persistence.h"
#include "waitlist.h"
#include "occupancy.h"
#include "venue_link.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            ++g_passed;                                                      \
        } else {                                                             \
            ++g_failed;                                                      \
            std::cout << "  FAIL  line " << __LINE__ << ": " #cond << '\n'; \
        }                                                                    \
    } while (0)

struct Rig {
    HashTable table;
    SeatGrid grid{Config::ROWS, Config::COLS};
    AllocationManager mgr{table, grid};
    WaitlistManager wl{table, grid, mgr};
    OccupancyMonitor mon{grid};
    campusflow::VenueGraph graph;

    Rig() { buildDefaultVenue(graph, mon, grid.zoneCount()); }

    void sync() { syncZonesToGraph(graph, mon, grid.zoneCount()); }

    int zoneNode(int z) const { return graph.findLocation("Zone " + std::to_string(z)); }

    void registerMany(int count, int firstIndex = 0) {
        for (int i = 0; i < count; ++i) {
            const int n = firstIndex + i;
            mgr.registerParticipant(makeParticipant("T" + std::to_string(1000 + n), "name", n % 3, n % Config::ZONES));
        }
    }

    std::vector<std::string> idsInState(ParticipantState st) {
        std::vector<std::string> ids;
        table.forEach([&](Participant& p) { if (p.state == st) ids.push_back(p.id); });
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    int bruteOccupiedInZone(int z) const {
        int a, b;
        grid.zoneRows(z, a, b);
        int n = 0;
        for (int r = a; r < b; ++r)
            for (int c = 0; c < grid.getCols(); ++c)
                if (grid.getSeat(r, c)->state == SeatState::OCCUPIED) ++n;
        return n;
    }

    int bruteUsableInZone(int z) const {
        int a, b;
        grid.zoneRows(z, a, b);
        int n = 0;
        for (int r = a; r < b; ++r)
            for (int c = 0; c < grid.getCols(); ++c)
                if (grid.getSeat(r, c)->state != SeatState::BLOCKED) ++n;
        return n;
    }
};

static void testDefaultVenueLinkedToTree() {
    std::cout << "[default venue is built from the segment tree]\n";
    Rig rig;
    CHECK(rig.graph.size() == 4 + rig.grid.zoneCount());
    for (int z = 0; z < rig.grid.zoneCount(); ++z) {
        const int id = rig.zoneNode(z);
        CHECK(id >= 0);
        CHECK(rig.graph.getLocation(id).capacity == rig.mon.zone(z).capacity);
        CHECK(rig.graph.getLocation(id).occupancy == 0);
    }
    CHECK(rig.graph.findLocation("Gate") >= 0);
    CHECK(rig.graph.findLocation("Exit East") >= 0);
    CHECK(rig.graph.findLocation("Exit West") >= 0);
    const auto bfs = rig.graph.reachableByBFS(rig.graph.findLocation("Gate"));
    CHECK(static_cast<int>(bfs.size()) == rig.graph.size());
}

static void testRegisterAllocateCheckInDrivesGraph() {
    std::cout << "[register -> allocate -> check-in -> graph occupancy]\n";
    Rig rig;
    rig.registerMany(16);
    for (int i = 0; i < 16; ++i) {
        Participant* p = rig.table.find("T" + std::to_string(1000 + i));
        p->preferredZone = 0;
    }
    int seated = 0;
    for (const auto& id : rig.idsInState(ParticipantState::REGISTERED))
        if (rig.wl.requestSeat(id) == SeatOutcome::SEATED) ++seated;
    CHECK(seated == 16);
    CHECK(rig.mgr.isConsistent());

    rig.sync();
    const int z0 = rig.zoneNode(0);
    CHECK(rig.graph.getLocation(z0).occupancy == 0);
    CHECK(!rig.graph.getLocation(z0).congestionAlertActive);

    for (const auto& id : rig.idsInState(ParticipantState::ALLOCATED))
        CHECK(rig.mgr.checkInSeat(id) == AllocResult::OK);
    rig.sync();
    CHECK(rig.graph.getLocation(z0).occupancy == 16);
    CHECK(rig.graph.getLocation(z0).occupancy == rig.bruteOccupiedInZone(0));
    CHECK(rig.graph.getLocation(z0).congestionAlertActive);
    CHECK(rig.graph.getLocation(rig.zoneNode(1)).occupancy == 0);
    CHECK(rig.mgr.isConsistent());
}

static void testHysteresisThroughTree() {
    std::cout << "[hysteresis driven by cancellations through the tree]\n";
    Rig rig;
    rig.registerMany(16);
    for (int i = 0; i < 16; ++i) rig.table.find("T" + std::to_string(1000 + i))->preferredZone = 0;
    for (const auto& id : rig.idsInState(ParticipantState::REGISTERED)) rig.wl.requestSeat(id);
    for (const auto& id : rig.idsInState(ParticipantState::ALLOCATED)) rig.mgr.checkInSeat(id);
    rig.sync();
    const int z0 = rig.zoneNode(0);
    CHECK(rig.graph.getLocation(z0).congestionAlertActive);

    const auto ids = rig.idsInState(ParticipantState::CHECKED_IN);
    CHECK(ids.size() == 16);
    rig.wl.cancel(ids[0]);
    rig.sync();
    CHECK(rig.graph.getLocation(z0).occupancy == 15);
    CHECK(rig.graph.getLocation(z0).congestionAlertActive);

    for (int i = 1; i <= 4; ++i) rig.wl.cancel(ids[static_cast<std::size_t>(i)]);
    rig.sync();
    CHECK(rig.graph.getLocation(z0).occupancy == 11);
    CHECK(!rig.graph.getLocation(z0).congestionAlertActive);
    CHECK(rig.mgr.isConsistent());
}

static void testWaitlistNoShowReassignmentFlow() {
    std::cout << "[full house -> waitlist -> no-show -> reassignment]\n";
    Rig rig;
    rig.registerMany(50);
    int seated = 0, waiting = 0;
    for (const auto& id : rig.idsInState(ParticipantState::REGISTERED)) {
        const SeatOutcome o = rig.wl.requestSeat(id);
        if (o == SeatOutcome::SEATED) ++seated;
        if (o == SeatOutcome::WAITLISTED) ++waiting;
    }
    CHECK(seated == 48);
    CHECK(waiting == 2);
    CHECK(rig.wl.waitingTotal() == 2);
    CHECK(rig.mon.venue().free == 0);
    CHECK(rig.mgr.isConsistent());

    int checkedIn = 0;
    for (const auto& id : rig.idsInState(ParticipantState::ALLOCATED)) {
        const Participant* p = rig.table.find(id);
        if (rig.grid.zoneOfRow(p->seatRow) == 0 && rig.mgr.checkInSeat(id) == AllocResult::OK) ++checkedIn;
    }
    CHECK(checkedIn == 16);
    CHECK(rig.wl.runNoShow() == -1);

    rig.wl.advanceClock(15);
    CHECK(rig.wl.graceOver());
    CHECK(rig.wl.runNoShow() == 32);
    CHECK(rig.wl.waitingTotal() == 0);
    CHECK(rig.wl.takeReassignments().size() == 2);
    CHECK(rig.mon.venue().checkedIn == 16);
    CHECK(rig.mon.venue().booked == 2);
    CHECK(rig.mon.venue().free == 30);
    CHECK(rig.idsInState(ParticipantState::NO_SHOW).size() == 32);
    CHECK(rig.mgr.isConsistent());
}

static void testPriorityOrderOnWaitlist() {
    std::cout << "[waitlist serves higher priority first, then earlier join]\n";
    Rig rig;
    for (int i = 0; i < 48; ++i)
        rig.mgr.registerParticipant(makeParticipant("F" + std::to_string(100 + i), "full", 0, i % 3));
    for (const auto& id : rig.idsInState(ParticipantState::REGISTERED)) rig.wl.requestSeat(id);
    CHECK(rig.mon.venue().free == 0);

    rig.mgr.registerParticipant(makeParticipant("W1", "low", 1, 0));
    rig.mgr.registerParticipant(makeParticipant("W2", "high", 9, 0));
    rig.mgr.registerParticipant(makeParticipant("W3", "high-late", 9, 0));
    CHECK(rig.wl.requestSeat("W1") == SeatOutcome::WAITLISTED);
    CHECK(rig.wl.requestSeat("W2") == SeatOutcome::WAITLISTED);
    CHECK(rig.wl.requestSeat("W3") == SeatOutcome::WAITLISTED);
    CHECK(rig.wl.requestSeat("W2") == SeatOutcome::ALREADY_WAITING);

    rig.wl.cancel("F100");
    CHECK(rig.table.find("W2")->hasSeat());
    CHECK(!rig.table.find("W3")->hasSeat());
    CHECK(!rig.table.find("W1")->hasSeat());
    rig.wl.cancel("F101");
    CHECK(rig.table.find("W3")->hasSeat());
    CHECK(!rig.table.find("W1")->hasSeat());
    rig.wl.cancel("F102");
    CHECK(rig.table.find("W1")->hasSeat());
    CHECK(rig.wl.waitingTotal() == 0);
    CHECK(rig.mgr.isConsistent());
}

static void testReassignKeepsCheckedInOccupancy() {
    std::cout << "[reassigning a checked-in person keeps them counted as present]\n";
    Rig rig;
    rig.mgr.registerParticipant(makeParticipant("Z1", "Zed", 5, 0));
    CHECK(rig.wl.requestSeat("Z1") == SeatOutcome::SEATED);
    CHECK(rig.mgr.checkInSeat("Z1") == AllocResult::OK);
    CHECK(rig.mon.venue().checkedIn == 1);
    CHECK(rig.mgr.reassignParticipant("Z1", 5, 5) == AllocResult::OK);
    CHECK(rig.table.find("Z1")->state == ParticipantState::CHECKED_IN);
    CHECK(rig.grid.getSeat(5, 5)->state == SeatState::OCCUPIED);
    CHECK(rig.grid.getSeat(0, 0)->state == SeatState::FREE);
    CHECK(rig.mon.venue().checkedIn == 1);
    CHECK(rig.mon.venue().booked == 0);
    CHECK(rig.mon.zone(0).checkedIn == 0);
    CHECK(rig.mon.zone(2).checkedIn == 1);
    CHECK(rig.mgr.isConsistent());

    rig.mgr.registerParticipant(makeParticipant("Z2", "Zoe", 5, 0));
    rig.wl.requestSeat("Z2");
    CHECK(rig.mgr.reassignParticipant("Z2", 4, 4) == AllocResult::OK);
    CHECK(rig.grid.getSeat(4, 4)->state == SeatState::ALLOCATED);
    CHECK(rig.mgr.isConsistent());
}

static void testEvacuationReroutesWhenExitCloses() {
    std::cout << "[evacuation reroutes when an exit closes]\n";
    Rig rig;
    const int z0 = rig.zoneNode(0);
    const int west = rig.graph.findLocation("Exit West");
    const int east = rig.graph.findLocation("Exit East");

    auto evac = rig.graph.multiSourceEvacuation();
    CHECK(evac.nextTowardExit[static_cast<std::size_t>(z0)] == west);

    rig.graph.setLocationOpen(west, false);
    evac = rig.graph.multiSourceEvacuation();
    CHECK(evac.nextTowardExit[static_cast<std::size_t>(z0)] != west);
    int cur = z0;
    for (int steps = 0; steps < rig.graph.size() && cur != east && cur != -1; ++steps)
        cur = evac.nextTowardExit[static_cast<std::size_t>(cur)];
    CHECK(cur == east);

    rig.graph.setLocationOpen(east, false);
    evac = rig.graph.multiSourceEvacuation();
    CHECK(evac.distance[static_cast<std::size_t>(z0)] == std::numeric_limits<double>::infinity());
}

static void testCongestionChangesRouteFromTreeData() {
    std::cout << "[tree occupancy changes the graph route]\n";
    Rig rig;
    const int gate = rig.graph.findLocation("Gate");
    const int east = rig.graph.findLocation("Exit East");
    const auto before = rig.graph.dijkstra(gate, false);
    const double emptyCost = before.distance[static_cast<std::size_t>(east)];

    for (int z = 0; z < rig.grid.zoneCount(); ++z)
        for (int i = 0; i < 16; ++i)
            rig.mgr.registerParticipant(makeParticipant("Z" + std::to_string(z) + "_" + std::to_string(i), "n", 0, z));
    for (const auto& id : rig.idsInState(ParticipantState::REGISTERED)) rig.wl.requestSeat(id);
    for (const auto& id : rig.idsInState(ParticipantState::ALLOCATED)) rig.mgr.checkInSeat(id);
    rig.sync();

    const auto after = rig.graph.dijkstra(gate, false);
    CHECK(after.distance[static_cast<std::size_t>(east)] > emptyCost);
    CHECK(rig.mgr.isConsistent());
}

static void testSaveLoadRoundTrip() {
    std::cout << "[save / load round trip keeps tree, grid and table consistent]\n";
    const std::string path = "integration_state.tmp";
    Rig a;
    a.registerMany(30);
    a.grid.blockSeat(5, 7);
    a.grid.reserveSeat(5, 6);
    for (const auto& id : a.idsInState(ParticipantState::REGISTERED)) a.wl.requestSeat(id);
    int n = 0;
    for (const auto& id : a.idsInState(ParticipantState::ALLOCATED))
        if (n++ % 2 == 0) a.mgr.checkInSeat(id);
    CHECK(a.mgr.isConsistent());
    CHECK(saveToFile(a.table, a.grid, path));

    Rig b;
    CHECK(loadFromFile(b.table, b.grid, path));
    CHECK(b.table.size() == a.table.size());
    CHECK(b.mgr.isConsistent());
    CHECK(b.grid.countOccupiedSeats() == a.grid.countOccupiedSeats());
    CHECK(b.grid.countAllocatedSeats() == a.grid.countAllocatedSeats());
    CHECK(b.grid.getSeat(5, 7)->state == SeatState::BLOCKED);
    CHECK(b.grid.getSeat(5, 6)->state == SeatState::RESERVED);
    for (int z = 0; z < a.grid.zoneCount(); ++z) {
        CHECK(b.mon.zone(z).checkedIn == a.mon.zone(z).checkedIn);
        CHECK(b.mon.zone(z).booked == a.mon.zone(z).booked);
        CHECK(b.mon.zone(z).free == a.mon.zone(z).free);
    }
    a.table.forEach([&](Participant& p) {
        const Participant* q = b.table.find(p.id);
        CHECK(q != nullptr);
        if (q) CHECK(q->seatRow == p.seatRow && q->seatCol == p.seatCol && q->state == p.state);
    });
    CHECK(!loadFromFile(b.table, b.grid, "no_such_file.tmp"));
    std::remove(path.c_str());
}

static void testRandomStressWithBruteForce() {
    std::cout << "[random stress: tree vs brute force, graph sync, consistency]\n";
    Rig rig;
    unsigned int state = 12345u;
    auto next = [&](unsigned int mod) {
        state = state * 1664525u + 1013904223u;
        return (state >> 8) % mod;
    };
    int nextId = 0;
    bool allConsistent = true;
    bool allMatch = true;

    for (int step = 0; step < 3000; ++step) {
        const unsigned int op = next(10);
        std::string id = "R" + std::to_string(next(120));
        if (op <= 2) {
            rig.mgr.registerParticipant(makeParticipant("R" + std::to_string(nextId++ % 120), "n", static_cast<int>(next(10)), static_cast<int>(next(3))));
        } else if (op <= 4) {
            rig.wl.requestSeat(id);
        } else if (op == 5) {
            rig.mgr.checkInSeat(id);
        } else if (op == 6) {
            rig.wl.cancel(id);
        } else if (op == 7) {
            rig.mgr.reassignParticipant(id, static_cast<int>(next(6)), static_cast<int>(next(8)));
        } else if (op == 8) {
            rig.wl.advanceClock(5);
            rig.wl.runNoShow();
        } else {
            rig.mgr.releaseSeat(id);
        }
        rig.wl.takeReassignments();

        if (step % 25 == 0) {
            if (!rig.mgr.isConsistent()) allConsistent = false;
            rig.sync();
            for (int z = 0; z < rig.grid.zoneCount(); ++z) {
                const auto& loc = rig.graph.getLocation(rig.zoneNode(z));
                if (loc.occupancy != rig.bruteOccupiedInZone(z)) allMatch = false;
                if (loc.capacity != std::max(1, rig.bruteUsableInZone(z))) allMatch = false;
                if (rig.mon.zone(z).checkedIn != rig.bruteOccupiedInZone(z)) allMatch = false;
            }
            if (rig.mon.venue().checkedIn != rig.grid.countOccupiedSeats()) allMatch = false;
            if (rig.mon.venue().booked != rig.grid.countAllocatedSeats()) allMatch = false;
        }
    }
    CHECK(allConsistent);
    CHECK(allMatch);
    CHECK(rig.mgr.isConsistent());
}

int main() {
    testDefaultVenueLinkedToTree();
    testRegisterAllocateCheckInDrivesGraph();
    testHysteresisThroughTree();
    testWaitlistNoShowReassignmentFlow();
    testPriorityOrderOnWaitlist();
    testReassignKeepsCheckedInOccupancy();
    testEvacuationReroutesWhenExitCloses();
    testCongestionChangesRouteFromTreeData();
    testSaveLoadRoundTrip();
    testRandomStressWithBruteForce();

    std::cout << "\nPassed: " << g_passed << "   Failed: " << g_failed << '\n';
    return g_failed == 0 ? 0 : 1;
}
