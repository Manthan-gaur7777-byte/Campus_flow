#pragma once
#include "VenueGraph.h"
#include <functional>

// Member 3's terminal menu, callable from the integrated program.
void customVenueSetup(campusflow::VenueGraph& graph);   // M3's manual setup (adds locations/corridors)
void runGraphMenu(campusflow::VenueGraph& graph, const std::function<void()>& beforeEachCommand);
