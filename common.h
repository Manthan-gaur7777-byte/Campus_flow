#pragma once
#include <string>

// ---- Shared types (agree on these with Members 2 and 3) ----
enum class SeatState { FREE, ALLOCATED, OCCUPIED, BLOCKED, RESERVED };

enum class ParticipantState { REGISTERED, ALLOCATED, CHECKED_IN, NO_SHOW, CANCELLED };

struct Seat {
    SeatState state = SeatState::FREE;
    std::string participantId;   // empty unless ALLOCATED / OCCUPIED
};

struct Participant {
    std::string id;
    std::string name;
    int priority = 0;            // higher = served first (used by Member 2's heap)
    int preferredZone = 0;       // zone to try first
    int seatRow = -1;            // -1 = no seat
    int seatCol = -1;
    ParticipantState state = ParticipantState::REGISTERED;

    bool hasSeat() const { return seatRow >= 0 && seatCol >= 0; }
};

namespace Config {
    constexpr int ROWS = 6;
    constexpr int COLS = 8;
    constexpr int ZONES = 3;          // zones are horizontal bands of rows
    constexpr int INITIAL_BUCKETS = 11;
    constexpr double MAX_LOAD = 0.75;
}

inline Participant makeParticipant(const std::string& id, const std::string& name,
                                   int priority = 0, int zone = 0) {
    Participant p; p.id = id; p.name = name; p.priority = priority; p.preferredZone = zone;
    return p;
}

inline const char* stateName(ParticipantState s) {
    switch (s) {
        case ParticipantState::REGISTERED: return "REGISTERED";
        case ParticipantState::ALLOCATED:  return "ALLOCATED";
        case ParticipantState::CHECKED_IN: return "CHECKED_IN";
        case ParticipantState::NO_SHOW:    return "NO_SHOW";
        case ParticipantState::CANCELLED:  return "CANCELLED";
    }
    return "?";
}
