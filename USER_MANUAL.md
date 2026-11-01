# CampusFlow — User Manual

CampusFlow is a terminal-only C++17 program for seating, check-in and crowd
movement at a venue. It has no server, no database and no web UI: you run one
program and use numbered menus.

It is built around five hand-written data structures:

| # | Structure | What it does here | Files |
|---|-----------|-------------------|-------|
| 1 | Hash table (chaining, auto-resize) | Finds a participant by ID | `hash_table.*` |
| 2 | 2D seat grid | Holds the state of every seat | `seat_grid.*`, `allocation.*` |
| 3 | Segment tree | Zone counts and "first free seat" in O(log N) | `segment_tree.*` |
| 4 | Min-heap | Waitlist (priority first, then join order) | `waitlist_heap.*`, `waitlist.*` |
| 5 | Graph | Venue map, congestion-aware routes, evacuation | `VenueGraph.h`, `graph_menu.*` |

---

## 1. Build and run

You need a compiler with C++17 support (g++ or MinGW-w64 on Windows). It was built and tested with g++ 13.3 on Linux.

**With make (Linux, macOS, MSYS2/MinGW):**

```
make            # builds ./campusflow
./campusflow    # run it (campusflow.exe on Windows)
make test       # builds and runs all 5 test suites
make demo       # replays the sample inputs and compares with expected output
make clean      # removes build files
```

**Without make (Windows PowerShell or any shell):**

```
g++ -std=c++17 -Wall -Wextra -O2 allocation.cpp hash_table.cpp persistence.cpp seat_grid.cpp segment_tree.cpp waitlist_heap.cpp waitlist.cpp occupancy.cpp graph_menu.cpp venue_link.cpp main.cpp -o campusflow
.\campusflow.exe
```

To build one test suite by hand, replace `main.cpp` with the test file, for
example `test_integration.cpp -o test_integration`.

To run a sample input instead of typing:

```
./campusflow < samples/full_demo_input.txt
```

---

## 2. How the program is organised

```
                +-----------------------------+
                |  main menu (main.cpp)       |
                +--+-----------+-----------+--+
                   |           |           |
        register / allocate    |       option 21 / 22
        check-in / no-show     |           |
                   v           v           v
   Hash table <-> Seat grid -> Segment tree      Venue graph
   (by ID)        (seat states)  (zone counts)   (locations, corridors)
                                      |                ^
                                      +-- venue_link --+
                                  tree numbers feed the graph
```

Key rules:

* Every seat change goes through the seat grid, which updates the segment tree
  at the same moment. Occupancy numbers shown anywhere come from **tree
  queries**, never from separate counters.
* Hash table and seat grid always agree: a participant's seat and the seat's
  participant ID point at each other. Option 23 checks this.
* The venue graph has one hall per seat zone, named `Zone 0`, `Zone 1`,
  `Zone 2`. Before every graph command, those halls get their capacity and
  occupancy from the segment tree. Only **checked-in** people count as present.

Fixed settings (`common.h`): 6 rows x 8 columns = 48 seats, 3 zones (two rows
each, 16 seats per zone), no-show grace period 15 minutes.

---

## 3. Main menu

Type the option number and press Enter. A non-number gives
`Please enter a whole number.`

### Participants and seats

| Option | What it does | Prompts |
|-------:|--------------|---------|
| 1 Register participant | Adds a participant with no seat yet. Rejected if the ID is empty or already used. | ID, Name, Priority (0-9), Preferred zone (0-2) |
| 2 Allocate seat | Gives a seat in the preferred zone (first free seat, found by the segment tree). If that zone is full it overflows to the nearest zone. If the whole venue is full the person joins the waitlist. | ID |
| 3 Check in | Seat goes from Allocated to Occupied. | ID |
| 4 Mark no-show | Frees the seat, marks the person NO_SHOW, and gives the seat to the next person on the waitlist. | ID |
| 5 Reassign seat | Moves a seated person to a specific free seat. The old seat is freed (and offered to the waitlist). If the new seat is taken, nothing changes. | ID, New row, New col |
| 6 Cancel registration | Removes the person from the waitlist and/or frees their seat, then promotes the next waiting person. | ID |
| 7 Show participant | Prints state, priority, preferred zone and seat. | ID |
| 18 Seat all registered | Tries to seat everyone who is REGISTERED without a seat, in ID order. Prints how many were seated and waitlisted. | none |

### Seats and zones

| Option | What it does |
|-------:|--------------|
| 8 Show seat grid | Grid with legend: `F` free, `A` allocated, `O` occupied, `X` blocked, `R` reserved. |
| 9 Zone statistics | Free seats and seats in use per zone. |
| 10 Block a seat | Takes a FREE seat out of service (e.g. broken). Blocked seats do not count towards capacity. |
| 11 Reserve a seat | Marks a FREE seat as reserved. Automatic allocation skips it. |

Row is 0-5 and column is 0-7. Only FREE seats can be blocked or reserved.

### Waitlist, clock and no-shows

| Option | What it does |
|-------:|--------------|
| 15 Show waitlist | One list per zone, best first: higher priority, then earlier join. |
| 16 Advance clock | Moves the simulated clock forward by N minutes. |
| 17 Run no-show check | Only works once the clock has reached 15 minutes. Releases every seat whose owner has not checked in and promotes waitlisted people into them. Prints each promotion. |
| 20 Simulate arrivals (zone) | Checks in everyone in the chosen zone who is still Allocated. |

### Reports and consistency

| Option | What it does |
|-------:|--------------|
| 19 Occupancy report | Per zone and for the whole venue: capacity, checked-in, booked, free, reserved, percentage and level. Levels: LOW below 30%, NORMAL below 70%, HIGH below 90%, OVERCROWDED 90% and up. |
| 23 Verify data consistency | Cross-checks hash table, seat grid and segment tree. Should always say *Consistent*. |

### Data and venue

| Option | What it does |
|-------:|--------------|
| 12 Load sample data | Registers N people `P101`, `P102`, ... with names, rotating priority 0-2 and rotating preferred zone. Loading twice skips duplicates. |
| 13 Save to file | Writes participants and blocked/reserved seats to a text file in the current folder. |
| 14 Load from file | Replaces current data with a saved file. A missing, corrupt or wrong-size file is rejected and nothing changes. |
| 21 Venue graph / evacuation | Opens the graph menu (section 4). |
| 22 Custom venue graph setup | Replaces the default venue with one you type in. |
| 0 Exit | Quits. |

---

## 4. Graph menu (main menu option 21)

The default venue: `Gate`, `Junction`, `Zone 0`, `Zone 1`, `Zone 2`,
`Exit East`, `Exit West`, joined by corridors. Names are not case-sensitive.

| Option | What it does |
|-------:|--------------|
| 1 Display venue and connections | All locations (occupancy, open/closed, alert) and corridors. |
| 2 Update a location's occupancy | Types a head-count by hand. |
| 3 Mark a location accessible/closed | Closes or reopens a location (for example an exit). |
| 4 Open/close a connection | Closes or reopens one corridor. |
| 5 Update a connection's congestion likelihood | 0-100%: how congestion-prone a corridor is. |
| 6 Fastest route (Dijkstra) | Route and time between two locations. Corridor cost is `normal time x (1 + likelihood x busiest end's occupancy ratio)`. Closed locations and corridors are avoided. |
| 7 Reachable locations (BFS) | Everything you can still reach from a start point. |
| 8 Best evacuation route | Route from a location to the cheapest open exit, using clearance times. |
| 9 Evacuation recommendations for all locations | Best exit for every location, from one multi-source search that starts at all open exits at once. |
| 10 Crowd/congestion analytics report | Per-location figures, counts of alerts, over-capacity locations and usable corridors. |
| 11 Simulate a crowd | Mode 1: set one location to 0-200% of capacity. Mode 2: random crowd for the whole venue from a seed (same seed gives the same crowd). Then prints recommendations. |
| 12 Show congestion recommendations | For each location with an active alert: hold or stop entries, nearest uncongested location with spare room to redirect people to, and the nearest open exit. |
| 0 Back to main menu | |

**Congestion alerts** switch on at 80% of capacity and switch off at 70%
(hysteresis, so they do not flicker). Between 70% and 80% an alert that is
already on stays on. Thresholds live in `Config.h`.

**Important:** the three zone halls are refreshed from the segment tree before
every graph command. A hand-typed or simulated occupancy on a `Zone` hall
therefore lasts for that one command only; the recommendations printed
straight after option 11 do use it. Use main menu options 3 and 20 to change
real zone occupancy. Other locations (Gate, Junction, exits, custom rooms)
keep what you type.

### Custom venue (main menu option 22)

Enter number of locations, then for each: name, type (1 Entrance, 2 Hall,
3 Junction, 4 Exit, 5 Other), capacity, current occupancy, accessible (y/n).
Then the number of corridors, and for each: two location names, normal travel
time (s), clearance time (s), congestion likelihood (0-100%), open (y/n).

To keep a custom venue linked to the seating system, name three halls exactly
`Zone 0`, `Zone 1` and `Zone 2`. Their capacity and occupancy are then taken
from the segment tree. Without those names the custom graph is standalone.
There is no way back to the default venue except restarting the program.

---

## 5. Sample inputs

Three scripted sessions live in `samples/`. Each `*_input.txt` is exactly what
you would type; each `*_expected_output.txt` is what the program prints.

```
make demo                        # runs all three and compares
UPDATE=1 sh samples/run_samples.sh   # regenerate expected files after an intended change
```

| Sample | What it exercises |
|--------|-------------------|
| `full_demo_input.txt` | The whole system start to finish (walkthrough below). |
| `custom_venue_input.txt` | Seating + custom venue linked to the tree via `Zone 0/1` names, alerts, recommendations, closures, BFS, simulate. |
| `edge_cases_input.txt` | Bad numbers, unknown IDs, duplicates, bad seats, early no-show check, unwritable and missing files, all exits closed, out-of-range simulate values. |

`make demo` also checks that the program exits cleanly (code 1, with a message)
if input ends in the middle of a graph prompt, and exits normally on empty input.

### Walkthrough of `full_demo_input.txt`

| Step | Menu | Expected result |
|------|------|-----------------|
| 1 | 1 `A100 Alice 5 1` | Registered. |
| 2 | 1 `A100` again | Rejected: ID already exists. |
| 3 | 12 → 50 | 50 participants registered (`P101`-`P150`). |
| 4 | 7 `P101`, 7 `NOPE` | Participant shown; second says not found. |
| 5 | 10 → (5,7); 11 → (5,6) | Seat blocked; seat reserved. 46 seats now usable. |
| 6 | 18 | **46 seated, 5 waitlisted** (51 people, 46 seats). |
| 7 | 8, 9, 15 | Grid full; zone stats; waitlist per zone ordered by priority. |
| 8 | 19, 23 | All zones 0% checked in; data consistent. |
| 9 | 20 → zone 0 | 16 people checked in; Zone 0 is 100% OVERCROWDED. |
| 10 | 21 → 1, 12 | Graph shows Zone 0 16/16 with alert; recommendation says hold entries, redirect to Zone 1. |
| 11 | 21 → 8 `Zone 0` | Evacuate via Exit West, cost 46.00 s. |
| 12 | 21 → 6 `Gate` `Exit East` | Gate → Junction → Zone 2 → Exit East, 52.00 s. |
| 13 | 21 → 7 `Gate` | BFS reaches all 7 locations. |
| 14 | 21 → 3 close `Exit West`, then 8 `Zone 0` | Route changes to Zone 0 → Zone 1 → Zone 2 → Exit East, 96.00 s. Then reopen. |
| 15 | 21 → 10, 11 (surge Gate 95%) | Analytics report; Gate alert with redirect advice. |
| 16 | 20 → zone 1 | 16 more checked in. |
| 17 | 16 → 10, then 17 | Grace period not over (10 < 15). |
| 18 | 16 → 5, then 17 | Clock 15. **14 no-show seats released; 5 waitlisted people seated** (priority order). |
| 19 | 15, 19, 23 | Waitlist empty; occupancy updated; consistent. |
| 20 | 5 `P148` → (4,0) | Reassigned. Grid shows the move. |
| 21 | 4 `P148`; 6 `P101`; 7 `P101` | No-show released; cancelled; P101 shows CANCELLED. |
| 22 | 13, 14 `demo_state.dat`, 23, 19 | Saved, loaded, still consistent. |
| 23 | 0 | Goodbye. |

---

## 6. Tests

`make test` builds and runs five suites. Each exits with code 0 only if every
check passes.

| Suite | Covers |
|-------|--------|
| `test_all` | Hash table (collisions, resize, duplicates), seat grid, allocation, zone overflow, tree/grid sync, save/load, edge cases. |
| `test_waitlist` | Priority and zone order, no-show release, a randomised run of about 1,600 people. |
| `test_occupancy` | Level thresholds, per-zone levels, edge cases, last-seat race, comparison against brute force. |
| `test_graph` (117 checks) | Graph validation, Dijkstra (disconnected, closed node/corridor, congestion cost changing the route), BFS, multi-source evacuation (multi-exit, closed exit, all exits closed), hysteresis, capacity updates. |
| `test_integration` (178 checks) | Whole system: registration to check-in feeding the graph; hysteresis driven by cancellations; waitlist and no-show flow; priority order; evacuation reroute; save/load round trip; 3,000-step random stress checking the tree against brute-force counts. |

---

## 7. Save file format and limits

* Save files are plain text, one line per participant (`P|id|name|priority|zone|row|col|state`) and per blocked or reserved seat. They load only into the same grid size (6 x 8).
* The **waitlist is not saved**. After loading, run option 18 to seat or re-waitlist everyone who has no seat.
* Names containing `|` are changed to a space when saving.
* Priority is expected to be 0-9 but is not range-checked.
* The grid size, zone count and grace period are compile-time settings in `common.h` and `waitlist.h`.

---

## 8. Troubleshooting

| Problem | Fix |
|---------|-----|
| `make: command not found` on Windows | Use the plain `g++` command in section 1, or install MSYS2/MinGW and use `mingw32-make`. |
| `Grace period not over yet` | Use option 16 to advance the clock to at least 15 minutes. |
| `That seat is not free` | The seat is taken, blocked or reserved. Check option 8. |
| `Could not load (missing, wrong size or corrupt)` | Check the file name and that it was saved by this program. |
| Typed occupancy on a Zone hall vanished | Expected: zone halls are refreshed from the tree (section 4). |
| A graph command ends the program with `Input ended` | Input ran out mid-prompt (for example a short input file). |

---

## 9. Viva cheat sheet

| Structure | Key operations | Complexity | Why this structure |
|-----------|----------------|------------|--------------------|
| Hash table | insert, find, erase by ID | O(1) average; resizes (doubles) when load reaches 0.75 | Direct lookup of a participant from an ID string. |
| Seat grid | seat state changes | O(1) per seat | Natural 2D layout; every change also updates the tree. |
| Segment tree | point update, range count, find first free seat | O(log N) each | Zone totals and "first free seat" without scanning all seats. |
| Min-heap | push, pop best | O(log W) | Always serves the highest-priority, earliest-joined waiting person. Cancel from the middle is O(W). |
| Graph | Dijkstra, BFS, multi-source evacuation | O((V+E) log V), O(V+E) | Models corridors; congestion makes corridors cost more; evacuation starts from all exits at once. |

Suggested demo order (about 5 minutes): replay steps 3-9 of the walkthrough
(registration, full house, waitlist, check-in), then step 10-14 (graph, alert,
exit closure and reroute), then 17-18 (no-show and automatic waitlist
promotion), and finish with option 23.
