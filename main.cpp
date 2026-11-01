#include "persistence.h"
#include "waitlist.h"
#include "occupancy.h"
#include "venue_link.h"
#include "graph_menu.h"
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <limits>

static const char* resultText(AllocResult r) {
    switch (r) {
        case AllocResult::OK:               return "Done.";
        case AllocResult::NOT_FOUND:        return "Participant not found.";
        case AllocResult::ALREADY_SEATED:   return "Participant already has a seat.";
        case AllocResult::NO_SEATS:         return "No free seats in any zone.";
        case AllocResult::INVALID_SEAT:     return "That seat position is outside the grid.";
        case AllocResult::SEAT_TAKEN:       return "That seat is not free (taken, blocked or reserved).";
        case AllocResult::NO_SEAT_ASSIGNED: return "Participant has no seat assigned.";
    }
    return "?";
}

static std::string askLine(const std::string& prompt) {
    std::cout << prompt; std::string s; std::getline(std::cin, s); return s;
}

static bool askInt(const std::string& prompt, int& v) {
    std::string s = askLine(prompt);
    try { size_t used = 0; v = std::stoi(s, &used); return used == s.size(); }
    catch (...) { std::cout << "Please enter a whole number.\n"; return false; }
}

static void showParticipant(const HashTable& t, const std::string& id) {
    const Participant* p = t.find(id);
    if (!p) { std::cout << "Participant not found.\n"; return; }
    std::cout << p->id << " | " << p->name << " | priority " << p->priority << " | pref zone "
              << p->preferredZone << " | " << stateName(p->state);
    if (p->hasSeat()) std::cout << " | seat (" << p->seatRow << "," << p->seatCol << ")";
    std::cout << '\n';
}

static void zoneStats(const SeatGrid& g) {
    for (int z = 0; z < g.zoneCount(); ++z)
        std::cout << "Zone " << z << ": free " << g.countFreeInZone(z) << ", in use "
                  << g.countUsedInZone(z) << '\n';
}

static void showReassignments(WaitlistManager& wl) {
    for (const auto& r : wl.takeReassignments())
        std::cout << "[waitlist] " << r.id << " moved from waitlist to seat (" << r.row << "," << r.col << ")\n";
}

static const char* outcomeText(SeatOutcome o) {
    switch (o) {
        case SeatOutcome::SEATED:          return "Seat allocated.";
        case SeatOutcome::WAITLISTED:      return "Venue full: added to the waitlist.";
        case SeatOutcome::ALREADY_SEATED:  return "Participant already has a seat.";
        case SeatOutcome::ALREADY_WAITING: return "Participant is already on the waitlist.";
        case SeatOutcome::NOT_FOUND:       return "Participant not found.";
    }
    return "?";
}

static void menu() {
    std::cout << "\n===== CampusFlow: Seating Core =====\n"
                 " 1 Register participant      8 Show seat grid\n"
                 " 2 Allocate seat             9 Zone statistics\n"
                 " 3 Check in                 10 Block a seat\n"
                 " 4 Mark no-show             11 Reserve a seat\n"
                 " 5 Reassign seat            12 Load sample data\n"
                 " 6 Cancel registration      13 Save to file\n"
                 " 7 Show participant         14 Load from file\n"
                 " 15 Show waitlist          16 Advance clock\n 17 Run no-show check       18 Seat all registered\n 19 Occupancy report         20 Simulate arrivals (zone)\n 21 Venue graph / evacuation 22 Custom venue graph setup\n 23 Verify data consistency\n 0 Exit\n";
}

int main() {
    HashTable table;
    SeatGrid grid(Config::ROWS, Config::COLS);
    AllocationManager mgr(table, grid);
    WaitlistManager wl(table, grid, mgr);   // registers the seat-freed hook
    OccupancyMonitor mon(grid);
    campusflow::VenueGraph graph;
    buildDefaultVenue(graph, mon, grid.zoneCount());
    auto syncGraph = [&]() { syncZonesToGraph(graph, mon, grid.zoneCount()); };

    for (;;) {
        menu();
        int choice;
        if (!askInt("Choice: ", choice)) { if (!std::cin) break; continue; }
        std::string id; int a = 0, b = 0, c = 0;
        switch (choice) {
            case 0: std::cout << "Goodbye.\n"; return 0;
            case 1: {
                id = askLine("ID: "); std::string name = askLine("Name: ");
                if (!askInt("Priority (0-9): ", a) || !askInt("Preferred zone (0-2): ", b)) break;
                std::cout << (mgr.registerParticipant(makeParticipant(id, name, a, b))
                              ? "Registered.\n" : "Rejected: ID is empty or already exists.\n");
                break; }
            case 2: id = askLine("ID: "); std::cout << outcomeText(wl.requestSeat(id)) << '\n'; break;
            case 3: id = askLine("ID: "); std::cout << resultText(mgr.checkInSeat(id)) << '\n'; break;
            case 4: id = askLine("ID: "); std::cout << resultText(mgr.releaseSeat(id)) << '\n'; showReassignments(wl); break;
            case 5: id = askLine("ID: ");
                if (askInt("New row: ", a) && askInt("New col: ", b))
                    std::cout << resultText(mgr.reassignParticipant(id, a, b)) << '\n';
                showReassignments(wl);
                break;
            case 6: id = askLine("ID: "); std::cout << resultText(wl.cancel(id)) << '\n'; showReassignments(wl); break;
            case 7: showParticipant(table, askLine("ID: ")); break;
            case 8: grid.display(); break;
            case 9: zoneStats(grid); break;
            case 10:
                if (askInt("Row: ", a) && askInt("Col: ", b)) {
                    std::cout << (grid.blockSeat(a, b) ? "Seat blocked.\n" : "Cannot block that seat.\n");
                }
                break;
            case 11:
                if (askInt("Row: ", a) && askInt("Col: ", b)) {
                    std::cout << (grid.reserveSeat(a, b) ? "Seat reserved.\n" : "Cannot reserve that seat.\n");
                }
                break;
            case 12:
                if (askInt("How many participants: ", c) && c > 0) {
                    std::cout << loadSampleData(mgr, c) << " participants registered.\n";
                }
                break;
            case 13: id = askLine("File name: ");
                std::cout << (saveToFile(table, grid, id) ? "Saved.\n" : "Could not write file.\n"); break;
            case 14: id = askLine("File name: ");
                std::cout << (loadFromFile(table, grid, id) ? "Loaded.\n" : "Could not load (missing, wrong size or corrupt).\n");
                break;
            case 15: wl.display(); break;
            case 16:
                if (askInt("Minutes to advance: ", a)) {
                    wl.advanceClock(a);
                    std::cout << "Clock = " << wl.clock() << " min (grace " << wl.grace() << " min).\n";
                }
                break;
            case 17: {
                int n = wl.runNoShow();
                if (n < 0) std::cout << "Grace period not over yet (clock " << wl.clock() << " < " << wl.grace() << ").\n";
                else std::cout << n << " no-show seat(s) released.\n";
                showReassignments(wl);
                break; }
            case 18: {
                std::vector<std::string> ids;
                table.forEach([&](Participant& p) { if (!p.hasSeat() && p.state == ParticipantState::REGISTERED) ids.push_back(p.id); });
                std::sort(ids.begin(), ids.end());
                int seated = 0, waiting = 0;
                for (const auto& pid : ids) {
                    SeatOutcome o = wl.requestSeat(pid);
                    if (o == SeatOutcome::SEATED) ++seated; else if (o == SeatOutcome::WAITLISTED) ++waiting;
                }
                std::cout << seated << " seated, " << waiting << " waitlisted.\n";
                break; }
            case 19: mon.display(wl.waitingTotal()); break;
            case 20:
                if (askInt("Zone to check in (0-2): ", a)) {
                    std::vector<std::string> ids;
                    table.forEach([&](Participant& p) {
                        if (p.state == ParticipantState::ALLOCATED && grid.zoneOfRow(p.seatRow) == a) ids.push_back(p.id);
                    });
                    int n = 0;
                    for (const auto& pid : ids) if (mgr.checkInSeat(pid) == AllocResult::OK) ++n;
                    std::cout << n << " people checked in to zone " << a << ".\n";
                }
                break;
            case 21: syncGraph(); runGraphMenu(graph, syncGraph); break;
            case 22: graph = campusflow::VenueGraph(); customVenueSetup(graph); syncGraph(); break;
            case 23: std::cout << (mgr.isConsistent()
                         ? "Consistent: hash table, seat grid and segment tree agree.\n"
                         : "INCONSISTENT: hash table, seat grid and segment tree disagree!\n");
                break;
            default: std::cout << "Unknown option.\n";
        }
    }
    return 0;
}
