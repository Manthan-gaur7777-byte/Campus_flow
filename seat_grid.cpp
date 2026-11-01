#include "seat_grid.h"
#include <iostream>
#include <algorithm>

SeatGrid::SeatGrid(int rows, int cols)
    : rows_(std::max(rows, 0)), cols_(std::max(cols, 0)),
      zones_(std::max(1, std::min(Config::ZONES, std::max(rows, 1)))),
      grid_(rows_, std::vector<Seat>(cols_)) {
    tree_.build(std::vector<TreeSeat>(static_cast<size_t>(rows_) * cols_, FREE_GENERAL));
}

TreeSeat SeatGrid::toTree(SeatState s) {
    switch (s) {
        case SeatState::FREE:      return FREE_GENERAL;
        case SeatState::RESERVED:  return FREE_RESERVED;
        case SeatState::ALLOCATED: return ALLOCATED;
        case SeatState::OCCUPIED:  return OCCUPIED;
        case SeatState::BLOCKED:   return BLOCKED;
    }
    return BLOCKED;
}

void SeatGrid::sync(int r, int c) { tree_.update(flatIndex(r, c), toTree(grid_[r][c].state)); }

int SeatGrid::findFirstFreeInZone(int zone) const {
    int a, b; zoneRows(zone, a, b);
    if (a >= b || cols_ == 0) return -1;
    return tree_.findFirstFree(a * cols_, b * cols_ - 1, GENERAL);
}

int SeatGrid::treeFreeInZone(int zone) const {
    int a, b; zoneRows(zone, a, b);
    if (a >= b || cols_ == 0) return 0;
    return tree_.query(a * cols_, b * cols_ - 1).freeGeneral;
}

Counts SeatGrid::zoneCounts(int zone) const {
    if (zone < 0 || zone >= zones_) return Counts();
    int a, b; zoneRows(zone, a, b);
    if (a >= b || cols_ == 0) return Counts();
    return tree_.query(a * cols_, b * cols_ - 1);
}

bool SeatGrid::treeMatchesGrid() const {
    for (int r = 0; r < rows_; ++r)
        for (int c = 0; c < cols_; ++c) {
            Counts k = tree_.query(flatIndex(r, c), flatIndex(r, c));
            TreeSeat t = toTree(grid_[r][c].state);
            int g = k.freeGeneral, rs = k.freeReserved, al = k.allocated, oc = k.occupied;
            bool ok = (t == FREE_GENERAL && g == 1 && !rs && !al && !oc) ||
                      (t == FREE_RESERVED && rs == 1 && !g && !al && !oc) ||
                      (t == ALLOCATED && al == 1 && !g && !rs && !oc) ||
                      (t == OCCUPIED && oc == 1 && !g && !rs && !al) ||
                      (t == BLOCKED && !g && !rs && !al && !oc);
            if (!ok) return false;
        }
    return true;
}

bool SeatGrid::isValidPosition(int r, int c) const {
    return r >= 0 && r < rows_ && c >= 0 && c < cols_;
}

int SeatGrid::zoneOfRow(int r) const {
    int per = std::max(1, rows_ / zones_);
    return std::min(r / per, zones_ - 1);
}

void SeatGrid::zoneRows(int zone, int& first, int& last) const {
    int per = std::max(1, rows_ / zones_);
    first = zone * per;
    last = (zone == zones_ - 1) ? rows_ : first + per;
}

Seat* SeatGrid::getSeat(int r, int c) {
    return isValidPosition(r, c) ? &grid_[r][c] : nullptr;
}
const Seat* SeatGrid::getSeat(int r, int c) const {
    return isValidPosition(r, c) ? &grid_[r][c] : nullptr;
}

bool SeatGrid::allocateSeat(int r, int c, const std::string& pid) {
    Seat* s = getSeat(r, c);
    if (!s || pid.empty() || s->state != SeatState::FREE) return false;
    s->state = SeatState::ALLOCATED;
    s->participantId = pid;
    sync(r, c);
    return true;
}

bool SeatGrid::freeSeat(int r, int c) {
    Seat* s = getSeat(r, c);
    if (!s) return false;
    if (s->state != SeatState::ALLOCATED && s->state != SeatState::OCCUPIED) return false;
    s->state = SeatState::FREE;
    s->participantId.clear();
    sync(r, c);
    return true;
}

bool SeatGrid::occupySeat(int r, int c) {
    Seat* s = getSeat(r, c);
    if (!s || s->state != SeatState::ALLOCATED) return false;
    s->state = SeatState::OCCUPIED;
    sync(r, c);
    return true;
}

bool SeatGrid::blockSeat(int r, int c) {
    Seat* s = getSeat(r, c);
    if (!s || s->state != SeatState::FREE) return false;
    s->state = SeatState::BLOCKED;
    sync(r, c);
    return true;
}

bool SeatGrid::reserveSeat(int r, int c) {
    Seat* s = getSeat(r, c);
    if (!s || s->state != SeatState::FREE) return false;
    s->state = SeatState::RESERVED;
    sync(r, c);
    return true;
}

int SeatGrid::countState(SeatState st, int r0, int r1) const {
    int n = 0;
    for (int r = r0; r < r1; ++r)
        for (int c = 0; c < cols_; ++c)
            if (grid_[r][c].state == st) ++n;
    return n;
}

int SeatGrid::countFreeSeats() const      { return countState(SeatState::FREE, 0, rows_); }
int SeatGrid::countAllocatedSeats() const { return countState(SeatState::ALLOCATED, 0, rows_); }
int SeatGrid::countOccupiedSeats() const  { return countState(SeatState::OCCUPIED, 0, rows_); }

int SeatGrid::countFreeInZone(int z) const { return treeFreeInZone(z); }
int SeatGrid::countUsedInZone(int z) const {
    int a, b; zoneRows(z, a, b);
    return countState(SeatState::ALLOCATED, a, b) + countState(SeatState::OCCUPIED, a, b);
}

void SeatGrid::display() const {
    std::cout << "Seat Grid\nF=Free A=Allocated O=Occupied X=Blocked R=Reserved\n   ";
    for (int c = 0; c < cols_; ++c) std::cout << c << ' ';
    std::cout << '\n';
    for (int r = 0; r < rows_; ++r) {
        std::cout << r << " |";
        for (int c = 0; c < cols_; ++c) {
            char ch = 'F';
            switch (grid_[r][c].state) {
                case SeatState::FREE: ch = 'F'; break;
                case SeatState::ALLOCATED: ch = 'A'; break;
                case SeatState::OCCUPIED: ch = 'O'; break;
                case SeatState::BLOCKED: ch = 'X'; break;
                case SeatState::RESERVED: ch = 'R'; break;
            }
            std::cout << ch << ' ';
        }
        std::cout << "  (zone " << zoneOfRow(r) << ")\n";
    }
}

void SeatGrid::reset() {
    for (auto& row : grid_) for (auto& seat : row) { seat.state = SeatState::FREE; seat.participantId.clear(); }
    tree_.build(std::vector<TreeSeat>(static_cast<size_t>(rows_) * cols_, FREE_GENERAL));
}
