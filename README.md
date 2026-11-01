# CampusFlow

Terminal-only C++17 program for venue seating, check-in and crowd movement,
built on five hand-written data structures: hash table, 2D seat grid,
segment tree, min-heap waitlist and venue graph.

```
make          # build ./campusflow
./campusflow  # run
make test     # 5 test suites
make demo     # replay samples/ and compare with expected output
```

Full instructions, every menu option, the sample walkthrough and a viva
cheat sheet are in **USER_MANUAL.md**. What changed when the three parts were
merged is in **MERGE_NOTES.md**.

## Ownership

| Member | Files |
|--------|-------|
| 1 | `common.h` `hash_table.*` `seat_grid.*` `allocation.*` `persistence.*` `test_all.cpp` |
| 2 | `segment_tree.*` `waitlist_heap.*` `waitlist.*` `occupancy.*` `test_waitlist.cpp` `test_occupancy.cpp` |
| 3 | `Config.h` `VenueGraph.h` `graph_menu.*` `test_graph.cpp` |
| Shared | `main.cpp` (main menu) `venue_link.*` (tree to graph sync) `test_integration.cpp` `Makefile` `samples/` |
