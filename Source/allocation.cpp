#include "allocation.h"

bool AllocationManager::registerParticipant(const Participant& p) {
    Participant copy = p;
    copy.seatRow = copy.seatCol = -1;
    copy.state = ParticipantState::REGISTERED;
    return table_.insert(copy);
}

bool AllocationManager::tryZone(Participant& p, int zone) {
    int idx = grid_.findFirstFreeInZone(zone);       // segment tree: O(log N)
    if (idx < 0) return false;
    int r = idx / grid_.getCols(), c = idx % grid_.getCols();
    if (!grid_.allocateSeat(r, c, p.id)) return false;
    p.seatRow = r; p.seatCol = c;
    p.state = ParticipantState::ALLOCATED;
    return true;
}

AllocResult AllocationManager::findAndAllocate(Participant& p, int zone) {
    if (p.hasSeat()) return AllocResult::ALREADY_SEATED;
    int zones = grid_.zoneCount();
    if (zone < 0 || zone >= zones) zone = 0;
    // preferred zone first, then overflow outward (nearest zones first)
    for (int d = 0; d < zones; ++d) {
        int lo = zone - d, hi = zone + d;
        if (lo >= 0 && tryZone(p, lo)) return AllocResult::OK;
        if (d > 0 && hi < zones && tryZone(p, hi)) return AllocResult::OK;
    }
    return AllocResult::NO_SEATS;
}

AllocResult AllocationManager::allocateParticipant(const std::string& id) {
    Participant* p = table_.find(id);
    if (!p) return AllocResult::NOT_FOUND;
    return findAndAllocate(*p, p->preferredZone);
}

AllocResult AllocationManager::cancelParticipant(const std::string& id) {
    Participant* p = table_.find(id);
    if (!p) return AllocResult::NOT_FOUND;
    if (p->hasSeat()) {
        int r = p->seatRow, c = p->seatCol;
        grid_.freeSeat(r, c);
        p->seatRow = p->seatCol = -1;
        p->state = ParticipantState::CANCELLED;
        notifyFreed(r, c);
    } else {
        p->state = ParticipantState::CANCELLED;
    }
    return AllocResult::OK;
}

AllocResult AllocationManager::reassignParticipant(const std::string& id, int r, int c) {
    Participant* p = table_.find(id);
    if (!p) return AllocResult::NOT_FOUND;
    if (!grid_.isValidPosition(r, c)) return AllocResult::INVALID_SEAT;
    if (!p->hasSeat()) return AllocResult::NO_SEAT_ASSIGNED;
    if (p->seatRow == r && p->seatCol == c) return AllocResult::OK;
    if (!grid_.allocateSeat(r, c, id)) return AllocResult::SEAT_TAKEN; // new seat first: old seat untouched on failure
    int oldR = p->seatRow, oldC = p->seatCol;
    grid_.freeSeat(oldR, oldC);
    p->seatRow = r; p->seatCol = c;
    if (p->state == ParticipantState::CHECKED_IN) grid_.occupySeat(r, c);
    notifyFreed(oldR, oldC);
    return AllocResult::OK;
}

AllocResult AllocationManager::checkInSeat(const std::string& id) {
    Participant* p = table_.find(id);
    if (!p) return AllocResult::NOT_FOUND;
    if (!p->hasSeat()) return AllocResult::NO_SEAT_ASSIGNED;
    grid_.occupySeat(p->seatRow, p->seatCol);
    p->state = ParticipantState::CHECKED_IN;
    return AllocResult::OK;
}

AllocResult AllocationManager::releaseSeat(const std::string& id) {
    Participant* p = table_.find(id);
    if (!p) return AllocResult::NOT_FOUND;
    if (!p->hasSeat()) return AllocResult::NO_SEAT_ASSIGNED;
    int r = p->seatRow, c = p->seatCol;
    grid_.freeSeat(r, c);
    p->seatRow = p->seatCol = -1;
    p->state = ParticipantState::NO_SHOW;
    notifyFreed(r, c);
    return AllocResult::OK;
}

bool AllocationManager::isConsistent() const {
    bool ok = grid_.treeMatchesGrid();
    // participant -> seat
    table_.forEach([&](Participant& p) {
        if (!p.hasSeat()) return;
        const Seat* s = grid_.getSeat(p.seatRow, p.seatCol);
        if (!s || s->participantId != p.id ||
            (s->state != SeatState::ALLOCATED && s->state != SeatState::OCCUPIED)) ok = false;
    });
    // seat -> participant
    for (int r = 0; r < grid_.getRows(); ++r)
        for (int c = 0; c < grid_.getCols(); ++c) {
            const Seat* s = grid_.getSeat(r, c);
            if (s->participantId.empty()) continue;
            const Participant* p = table_.find(s->participantId);
            if (!p || p->seatRow != r || p->seatCol != c) ok = false;
        }
    return ok;
}
