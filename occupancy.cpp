#include "occupancy.h"
#include <iomanip>
#include <iostream>

const char* levelName(OccLevel l) {
    switch (l) {
        case OccLevel::LOW:         return "LOW";
        case OccLevel::NORMAL:      return "NORMAL";
        case OccLevel::HIGH:        return "HIGH";
        case OccLevel::OVERCROWDED: return "OVERCROWDED";
    }
    return "?";
}

OccLevel OccupancyMonitor::classify(double pct) const {
    if (pct >= th_.overFrom)   return OccLevel::OVERCROWDED;   // boundaries belong to the higher level
    if (pct >= th_.highFrom)   return OccLevel::HIGH;
    if (pct >= th_.normalFrom) return OccLevel::NORMAL;
    return OccLevel::LOW;
}

ZoneOccupancy OccupancyMonitor::fromCounts(int zone, const Counts& k) const {
    ZoneOccupancy z;
    z.zone = zone;
    z.checkedIn = k.occupied;
    z.booked = k.allocated;
    z.free = k.freeGeneral;
    z.reserved = k.freeReserved;
    z.capacity = k.freeGeneral + k.freeReserved + k.allocated + k.occupied;
    z.pct = z.capacity > 0 ? double(z.checkedIn) / z.capacity : 0.0;   // no division by zero
    z.level = classify(z.pct);
    return z;
}

ZoneOccupancy OccupancyMonitor::zone(int z) const { return fromCounts(z, grid_.zoneCounts(z)); }

ZoneOccupancy OccupancyMonitor::venue() const {
    Counts total;
    for (int z = 0; z < grid_.zoneCount(); ++z) total = merge(total, grid_.zoneCounts(z));
    return fromCounts(-1, total);
}

void OccupancyMonitor::display(int waitingCount) const {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Zone | Capacity | Checked-in | Booked | Free | Reserved | Occupancy | Level\n";
    for (int z = 0; z < grid_.zoneCount(); ++z) {
        ZoneOccupancy o = zone(z);
        std::cout << std::setw(4) << z << " | " << std::setw(8) << o.capacity << " | " << std::setw(10) << o.checkedIn
                  << " | " << std::setw(6) << o.booked << " | " << std::setw(4) << o.free << " | " << std::setw(8) << o.reserved
                  << " | " << std::setw(8) << o.pct * 100 << "% | " << levelName(o.level) << '\n';
    }
    ZoneOccupancy v = venue();
    std::cout << " ALL | " << std::setw(8) << v.capacity << " | " << std::setw(10) << v.checkedIn << " | " << std::setw(6) << v.booked
              << " | " << std::setw(4) << v.free << " | " << std::setw(8) << v.reserved << " | " << std::setw(8) << v.pct * 100
              << "% | " << levelName(v.level) << '\n';
    std::cout << "Waitlist length: " << waitingCount << '\n';
}
