# CampusFlow — Member 3 Graph Module

## Files
- `Config.h`: configurable congestion-alert thresholds.
- `VenueGraph.h`: adjacency-list graph, Dijkstra, BFS, multi-source evacuation routing, closure and occupancy updates, congestion reporting.
- `main.cpp`: interactive terminal setup and menu.
- `campusflow_sample_input.txt`: sample answers to exercise a small venue and check the route outputs.

## Compile
Use a C++17-capable compiler (GCC 16.3 is fine):

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow main.cpp -o campusflow
```

Run interactively:

```bash
./campusflow
```

On Windows PowerShell, run `./campusflow.exe` after compiling with the executable name adjusted for your compiler.

To replay the sample flow in a shell:

```bash
./campusflow < campusflow_sample_input.txt
```

## Cost model
For a corridor, the normal-routing edge cost is:

`normalTimeSeconds * (1 + congestionLikelihoodPercent / 100 * max(endpoint occupancy ratios))`

Corridor distance is not collected because routing is based on estimated travel
time only.

The occupancy ratio of a location is `occupancy / capacity`. Occupancy is allowed to exceed capacity so overloads are visible in the report.

Normal Dijkstra uses the corridor's normal travel time. Evacuation recommendations use its separately entered clearance-time estimate. The congestion multiplier is an explicitly documented heuristic; it is not real-time sensor data or a scientifically calibrated prediction.

## Current assumptions
- Every corridor is bidirectional in this first version.
- Locations and corridors are entered once at startup, then modified through the menu.
- Occupancy is manually entered for standalone testing. It must be connected to the team's shared occupancy/segment-tree interface during integration.
- Congestion alert defaults are 80% to activate and 70% to clear. These defaults are in `Config.h` and use hysteresis to avoid alerts rapidly switching on/off around one threshold.
- Data is kept in memory and is lost when the program exits. Persistence is not included in this first version.
