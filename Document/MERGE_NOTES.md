# Merge notes

Sources merged: `campus_flow.zip` (Member 3's latest graph module and tests) and
`campusflow_merged.zip` (Members 1 and 2 plus the first glue code).

## What was missing or broken in the merged zip

| Problem | Fix |
|---------|-----|
| `VenueGraph.h` was not in the merged zip, so nothing that used the graph could build. | Added Member 3's `VenueGraph.h`. |
| `venue_link.cpp` calls `VenueGraph::updateCapacity()`, which did not exist. | Added `updateCapacity(id, capacity)` to `VenueGraph.h` (rejects capacity <= 0, refreshes the congestion alert). |
| `graph_menu.cpp` was an older copy of Member 3's `main.cpp`. It looped forever when input ended (verified), had no single-location handling, and had no options 11 and 12. | Rebuilt `graph_menu.cpp` from Member 3's latest `main.cpp`, keeping `customVenueSetup` and `runGraphMenu` so `main.cpp` and `graph_menu.h` did not change. Menu now has options 11 (simulate a crowd) and 12 (congestion recommendations). |
| Reassigning a checked-in person left their state `CHECKED_IN` but made the new seat `ALLOCATED`, so the occupancy report dropped them from "checked in". | One line in `AllocationManager::reassignParticipant` (`allocation.cpp`): a checked-in person's new seat is marked occupied. Covered by `test_integration`. |
| Reassign (menu 5) freed a seat and promoted a waitlisted person without telling the user. | `main.cpp` option 5 now prints the promotion like options 4, 6 and 17 do. |
| The tree/grid/table consistency check existed in code but had no menu entry. | Added main menu option 23. |
| A zone at exactly 100% was described as "near capacity". | Recommendation text now says "at full capacity" at exactly 100%. |
| `segment_tree.*` used Windows line endings while every other file used Unix. | Converted to Unix line endings. No code change. |
| No Makefile, no integration test, no scripted samples. | Added `Makefile`, `test_integration.cpp`, `samples/`, `USER_MANUAL.md`. |
| `test_graph.cpp` had no test for the new function. | Added `test_update_capacity`. |

Executables (`*.exe`) from the Member 3 zip were not carried over; `.gitignore` excludes them.

## Not changed, but worth knowing before the viva

* **The "write it yourself" rule.** The hash table, segment tree, heap and graph
  are hand-written, but some parts lean on standard containers:
  `HashTable` uses `std::list` for its buckets, `WaitlistManager` keeps a
  `std::unordered_set` of waiting IDs, and `VenueGraph` uses `std::unordered_map`
  (name lookup), `std::priority_queue` (Dijkstra) and `std::queue` (BFS).
  If your examiner reads the rule strictly, be ready to explain these, or swap
  them for your own heap and hash table.
* **Waitlist is not saved** by option 13. After loading, run option 18.
* **Graph option 2 / 11 on `Zone` halls** last one command only, because zone
  halls are refreshed from the segment tree before every graph command.
* **Option 22 replaces the default venue** and there is no way back to it
  without restarting.
* `Config` exists twice with different meanings: `namespace Config` in
  `common.h` (grid size, hash settings) and `campusflow::Config` in `Config.h`
  (congestion thresholds). They live in different namespaces and do not clash,
  but avoid `using namespace campusflow;` in files that include `common.h`.

## Verification done on the merged project

* Builds with no warnings under `-Wall -Wextra` (g++ 13.3).
* All five test suites pass (`make test`): `test_all`, `test_waitlist`,
  `test_occupancy`, `test_graph` (117 checks), `test_integration` (178 checks).
* The integration tests were checked by deliberately breaking the code (graph
  reads the wrong tree number; seat grid forgets to update the tree; waitlist
  ignores priority; reassign fix removed). Each break made tests fail.
* `make demo` replays three sample sessions and two end-of-input cases and
  compares them with saved output; a deliberate waitlist-order break makes it fail.
* Not tested: Windows. Build and run there with the `g++` command in the manual.
