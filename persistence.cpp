#include "persistence.h"
#include <fstream>
#include <sstream>
#include <vector>

static const char* const NAMES[] = {"Asha", "Ravi", "Meena", "Karthik", "Divya", "Imran", "Sneha",
                                    "Arjun", "Pooja", "Vikram", "Nisha", "Rahul"};

int loadSampleData(AllocationManager& mgr, int count) {
    int ok = 0;
    const int nNames = sizeof(NAMES) / sizeof(NAMES[0]);
    for (int i = 0; i < count; ++i) {
        std::string id = "P" + std::to_string(101 + i);
        Participant p = makeParticipant(id, NAMES[i % nNames], i % 3, i % Config::ZONES);
        if (mgr.registerParticipant(p)) ++ok;           // duplicates (re-loading) are skipped
    }
    return ok;
}

static std::string clean(std::string s) {               // keep the '|' separator unambiguous
    for (char& c : s) if (c == '|' || c == '\n' || c == '\r') c = ' ';
    return s;
}

bool saveToFile(const HashTable& table, const SeatGrid& grid, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "CAMPUSFLOW 1 " << grid.getRows() << ' ' << grid.getCols() << '\n';
    for (int r = 0; r < grid.getRows(); ++r)
        for (int c = 0; c < grid.getCols(); ++c) {
            SeatState st = grid.getSeat(r, c)->state;
            if (st == SeatState::BLOCKED)  out << "S|" << r << '|' << c << "|X\n";
            if (st == SeatState::RESERVED) out << "S|" << r << '|' << c << "|R\n";
        }
    const_cast<HashTable&>(table).forEach([&](Participant& p) {
        out << "P|" << clean(p.id) << '|' << clean(p.name) << '|' << p.priority << '|'
            << p.preferredZone << '|' << p.seatRow << '|' << p.seatCol << '|'
            << static_cast<int>(p.state) << '\n';
    });
    return static_cast<bool>(out);
}

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> f; std::string cur; std::istringstream in(line);
    while (std::getline(in, cur, '|')) f.push_back(cur);
    return f;
}

static bool toInt(const std::string& s, int& v) {
    try { size_t used = 0; v = std::stoi(s, &used); return used == s.size(); }
    catch (...) { return false; }
}

bool loadFromFile(HashTable& table, SeatGrid& grid, const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line, tag; int ver, rows, cols;
    if (!std::getline(in, line)) return false;
    std::istringstream hdr(line);
    if (!(hdr >> tag >> ver >> rows >> cols) || tag != "CAMPUSFLOW" || ver != 1) return false;
    if (rows != grid.getRows() || cols != grid.getCols()) return false;

    // Parse everything first so a corrupt file cannot leave a half-loaded state.
    struct SeatRec { int r, c; char kind; };
    std::vector<SeatRec> seats; std::vector<Participant> people;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto f = split(line);
        if (f[0] == "S" && f.size() == 4) {
            SeatRec s; if (!toInt(f[1], s.r) || !toInt(f[2], s.c) || !grid.isValidPosition(s.r, s.c)
                          || (f[3] != "X" && f[3] != "R")) return false;
            s.kind = f[3][0]; seats.push_back(s);
        } else if (f[0] == "P" && f.size() == 8) {
            Participant p; int st;
            if (f[1].empty() || !toInt(f[3], p.priority) || !toInt(f[4], p.preferredZone) ||
                !toInt(f[5], p.seatRow) || !toInt(f[6], p.seatCol) || !toInt(f[7], st)) return false;
            if (st < 0 || st > 4) return false;
            p.id = f[1]; p.name = f[2]; p.state = static_cast<ParticipantState>(st);
            if (p.hasSeat() && !grid.isValidPosition(p.seatRow, p.seatCol)) return false;
            people.push_back(p);
        } else return false;
    }

    table.clear(); grid.reset();
    for (auto& s : seats) { if (s.kind == 'X') grid.blockSeat(s.r, s.c); else grid.reserveSeat(s.r, s.c); }
    for (auto& p : people) {
        Participant q = p;
        bool seated = p.hasSeat();
        q.seatRow = q.seatCol = -1;
        if (!table.insert(q)) return false;                       // duplicate ID in file
        if (seated) {
            if (!grid.allocateSeat(p.seatRow, p.seatCol, p.id)) return false;   // double-booked seat
            if (p.state == ParticipantState::CHECKED_IN) grid.occupySeat(p.seatRow, p.seatCol);
            Participant* stored = table.find(p.id);
            stored->seatRow = p.seatRow; stored->seatCol = p.seatCol;
        }
    }
    return true;
}
