#pragma once
#include "VenueGraph.h"
#include "occupancy.h"

// Glue between the seating core (segment tree) and Member 3's venue graph.
// Zone halls are named "Zone 0", "Zone 1", ... and take capacity/occupancy from the tree.
void buildDefaultVenue(campusflow::VenueGraph& g, const OccupancyMonitor& mon, int zones);
void syncZonesToGraph(campusflow::VenueGraph& g, const OccupancyMonitor& mon, int zones);
