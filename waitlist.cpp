#include "waitlist.h"
#include <iostream>

WaitlistManager::WaitlistManager(HashTable& t, SeatGrid& g, AllocationManager& m, int grace)
    : table_(t), grid_(g), mgr_(m), heaps_(g.zoneCount()), grace_(grace) {
    mgr_.setSeatFreedHook([this](int r, int c) { onSeatFreed(r, c); });
}

SeatOutcome WaitlistManager::requestSeat(const std::string& id) {
    Participant* p = table_.find(id);
    if (!p) return SeatOutcome::NOT_FOUND;
    if (p->hasSeat()) return SeatOutcome::ALREADY_SEATED;
    if (waiting_.count(id)) return SeatOutcome::ALREADY_WAITING;
    if (mgr_.allocateParticipant(id) == AllocResult::OK) return SeatOutcome::SEATED;
    int z = p->preferredZone;                          // venue is full: join preferred zone's heap
    if (z < 0 || z >= (int)heaps_.size()) z = 0;
    heaps_[z].push({id, p->priority, seq_++});
    waiting_.insert(id);
    return SeatOutcome::WAITLISTED;
}

AllocResult WaitlistManager::cancel(const std::string& id) {
    if (!table_.find(id)) return AllocResult::NOT_FOUND;
    if (waiting_.erase(id)) {
        for (auto& h : heaps_) if (h.removeById(id)) break;
    }
    return mgr_.cancelParticipant(id);                 // if it held a seat, the hook refills it
}

int WaitlistManager::bestZone(int preferred) const {
    if (!heaps_[preferred].empty()) return preferred;  // same zone first
    int best = -1;
    for (int z = 0; z < (int)heaps_.size(); ++z) {
        if (heaps_[z].empty()) continue;
        if (best < 0 || WaitlistHeap::better(heaps_[z].top(), heaps_[best].top())) best = z;
    }
    return best;
}

void WaitlistManager::onSeatFreed(int r, int /*c*/) {
    int zone = grid_.zoneOfRow(r);
    for (;;) {
        int z = bestZone(zone);
        if (z < 0) return;                             // nobody waiting: seat just stays FREE
        WaitEntry e = heaps_[z].pop();
        waiting_.erase(e.id);
        Participant* p = table_.find(e.id);
        if (!p || p->hasSeat() || p->state == ParticipantState::CANCELLED) continue;   // stale entry
        if (mgr_.findAndAllocate(*p, zone) == AllocResult::OK) {
            log_.push_back({p->id, p->seatRow, p->seatCol});
            return;
        }
        heaps_[z].push(e); waiting_.insert(e.id);      // could not seat: put back, stop
        return;
    }
}

int WaitlistManager::runNoShow() {
    if (!graceOver()) return -1;
    std::vector<std::string> ids;                      // collect first: reassignment changes states mid-run
    table_.forEach([&](Participant& p) { if (p.state == ParticipantState::ALLOCATED) ids.push_back(p.id); });
    int released = 0;
    for (const auto& id : ids)
        if (mgr_.releaseSeat(id) == AllocResult::OK) ++released;   // hook reassigns each freed seat
    return released;
}

int WaitlistManager::waitingInZone(int zone) const {
    return (zone >= 0 && zone < (int)heaps_.size()) ? heaps_[zone].size() : 0;
}

void WaitlistManager::display() const {
    std::cout << "Waitlist (" << waitingTotal() << " waiting)\n";
    for (int z = 0; z < (int)heaps_.size(); ++z) {
        WaitlistHeap copy = heaps_[z];                 // pop a copy so the real heap is untouched
        std::cout << " Zone " << z << ": " << copy.size() << " waiting\n";
        for (int i = 1; !copy.empty(); ++i) {
            WaitEntry e = copy.pop();
            std::cout << "   " << i << ". " << e.id << " (priority " << e.weight << ", joined #" << e.regTime << ")\n";
        }
    }
}

std::vector<Reassignment> WaitlistManager::takeReassignments() {
    std::vector<Reassignment> out;
    out.swap(log_);
    return out;
}
