#include "venue_link.h"
#include <algorithm>
#include <string>

static std::string zoneName(int z) { return "Zone " + std::to_string(z); }

void syncZonesToGraph(campusflow::VenueGraph& g, const OccupancyMonitor& mon, int zones) {
    for (int z = 0; z < zones; ++z) {
        int id = g.findLocation(zoneName(z));
        if (id < 0) continue;                                  // custom venue without this zone: skip
        ZoneOccupancy o = mon.zone(z);                         // O(log N) tree query
        g.updateCapacity(id, std::max(1, o.capacity));
        g.updateOccupancy(id, o.checkedIn);                    // people physically present
    }
}

void buildDefaultVenue(campusflow::VenueGraph& g, const OccupancyMonitor& mon, int zones) {
    g.addLocation("Gate", "Entrance", 200, 0, true);
    g.addLocation("Junction", "Junction", 100, 0, true);
    for (int z = 0; z < zones; ++z) g.addLocation(zoneName(z), "Hall", std::max(1, mon.zone(z).capacity), 0, true);
    g.addLocation("Exit East", "Exit", 150, 0, true);
    g.addLocation("Exit West", "Exit", 150, 0, true);

    // addConnection(u, v, normal s, clearance s, congestion-proneness %, open)
    int gate = g.findLocation("Gate"), junc = g.findLocation("Junction");
    int east = g.findLocation("Exit East"), west = g.findLocation("Exit West");
    g.addConnection(gate, junc, 12, 28, 40, true);
    for (int z = 0; z < zones; ++z) g.addConnection(junc, g.findLocation(zoneName(z)), 18, 45, 60, true);
    for (int z = 0; z + 1 < zones; ++z)                        // cross passages between neighbouring zones
        g.addConnection(g.findLocation(zoneName(z)), g.findLocation(zoneName(z + 1)), 10, 20, 30, true);
    g.addConnection(g.findLocation(zoneName(0)), west, 24, 40, 15, true);
    g.addConnection(g.findLocation(zoneName(zones - 1)), east, 22, 50, 20, true);
    syncZonesToGraph(g, mon, zones);
}
