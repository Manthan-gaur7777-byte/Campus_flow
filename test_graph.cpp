#include "VenueGraph.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using campusflow::Config;
using campusflow::VenueGraph;

static int g_passed = 0;
static int g_failed = 0;
static const double INF = std::numeric_limits<double>::infinity();

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            ++g_passed;                                                      \
        } else {                                                             \
            ++g_failed;                                                      \
            std::cout << "  FAIL  line " << __LINE__ << ": " #cond << '\n'; \
        }                                                                    \
    } while (0)

static bool near(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

static std::vector<int> pathTo(int src, int dst, const std::vector<int>& parent) {
    std::vector<int> p;
    int cur = dst;
    for (int steps = 0; cur != -1 && steps <= static_cast<int>(parent.size()); ++steps) {
        p.insert(p.begin(), cur);
        if (cur == src) return p;
        cur = parent[static_cast<std::size_t>(cur)];
    }
    return {};
}

static VenueGraph makeDiamond() {
    VenueGraph g;
    g.addLocation("Gate", "Entrance", 100, 0, true);
    g.addLocation("Hall", "Hall", 100, 0, true);
    g.addLocation("Corr", "Junction", 100, 0, true);
    g.addLocation("ExitE", "Exit", 100, 0, true);
    g.addConnection(0, 1, 10, 10, 0, true);
    g.addConnection(1, 3, 10, 10, 0, true);
    g.addConnection(0, 2, 5, 5, 0, true);
    g.addConnection(2, 3, 30, 30, 0, true);
    return g;
}

static void testBuildAndValidation() {
    std::cout << "[build / validation]\n";
    VenueGraph g;
    CHECK(g.size() == 0);
    CHECK(g.addLocation("Gate", "Entrance", 100, 10, true));
    CHECK(!g.addLocation("gate", "Hall", 50, 0, true));
    CHECK(!g.addLocation("  GATE  ", "Hall", 50, 0, true));
    CHECK(!g.addLocation("", "Hall", 50, 0, true));
    CHECK(!g.addLocation("X", "Hall", 0, 0, true));
    CHECK(!g.addLocation("Y", "Hall", -5, 0, true));
    CHECK(!g.addLocation("Z", "Hall", 10, -1, true));
    CHECK(g.size() == 1);
    CHECK(g.findLocation("GATE") == 0);
    CHECK(g.findLocation("  gate ") == 0);
    CHECK(g.findLocation("nowhere") == -1);

    g.addLocation("Hall", "Hall", 100, 0, true);
    CHECK(g.addConnection(0, 1, 10, 20, 50, true));
    CHECK(!g.addConnection(0, 1, 10, 20, 50, true));
    CHECK(!g.addConnection(1, 0, 10, 20, 50, true));
    CHECK(!g.addConnection(0, 0, 10, 20, 50, true));
    CHECK(!g.addConnection(0, 9, 10, 20, 50, true));
    CHECK(!g.addConnection(-1, 1, 10, 20, 50, true));
    CHECK(g.hasConnection(0, 1) && g.hasConnection(1, 0));
    CHECK(g.getEdges(0).size() == 1 && g.getEdges(1).size() == 1);

    g.addLocation("A", "Hall", 10, 0, true);
    CHECK(!g.addConnection(0, 2, 0, 5, 0, true));
    CHECK(!g.addConnection(0, 2, 5, 0, 0, true));
    CHECK(!g.addConnection(0, 2, -1, 5, 0, true));
    CHECK(!g.addConnection(0, 2, 5, 5, -1, true));
    CHECK(!g.addConnection(0, 2, 5, 5, 101, true));
    CHECK(g.addConnection(0, 2, 5, 5, 0, true));
}

static void testConfigFallback() {
    std::cout << "[config validation]\n";
    Config bad;
    bad.highCongestionThresholdRatio = 0.5;
    bad.clearCongestionThresholdRatio = 0.9;
    VenueGraph g(bad);
    g.addLocation("A", "Hall", 100, 0, true);
    g.updateOccupancy(0, 80);
    CHECK(g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 70);
    CHECK(!g.getLocation(0).congestionAlertActive);
}

static void testDijkstraBasic() {
    std::cout << "[dijkstra basic]\n";
    VenueGraph g = makeDiamond();
    auto r = g.dijkstra(0);
    CHECK(near(r.distance[0], 0));
    CHECK(near(r.distance[1], 10));
    CHECK(near(r.distance[2], 5));
    CHECK(near(r.distance[3], 20));
    std::vector<int> p = pathTo(0, 3, r.parent);
    CHECK((p == std::vector<int>{0, 1, 3}));
    CHECK(pathTo(0, 0, r.parent) == std::vector<int>{0});
}

static void testDijkstraDisconnected() {
    std::cout << "[dijkstra disconnected]\n";
    VenueGraph g = makeDiamond();
    g.addLocation("Island", "Hall", 10, 0, true);
    auto r = g.dijkstra(0);
    CHECK(r.distance[4] == INF);
    CHECK(pathTo(0, 4, r.parent).empty());
}

static void testDijkstraClosedNodes() {
    std::cout << "[dijkstra closed node / edge]\n";
    VenueGraph g = makeDiamond();

    g.setLocationOpen(1, false);
    auto r = g.dijkstra(0);
    CHECK(near(r.distance[3], 35));
    CHECK((pathTo(0, 3, r.parent) == std::vector<int>{0, 2, 3}));
    CHECK(r.distance[1] == INF);

    g.setLocationOpen(1, true);
    g.setConnectionOpen(0, 1, false);
    r = g.dijkstra(0);
    CHECK(near(r.distance[3], 35));
    CHECK(near(r.distance[1], 45));

    g.setConnectionOpen(0, 2, false);
    r = g.dijkstra(0);
    CHECK(r.distance[3] == INF);

    g.setConnectionOpen(0, 2, true);
    g.setLocationOpen(0, false);
    r = g.dijkstra(0);
    CHECK(r.distance[0] == INF && r.distance[3] == INF);

    r = g.dijkstra(99);
    CHECK(r.distance[0] == INF);
}

static void testCongestionCost() {
    std::cout << "[congestion cost]\n";
    VenueGraph g;
    g.addLocation("A", "Hall", 100, 0, true);
    g.addLocation("B", "Hall", 100, 0, true);
    g.addConnection(0, 1, 10, 20, 50, true);
    const auto& e = g.getEdges(0)[0];

    CHECK(near(g.estimatedCost(0, e), 10.0));
    g.updateOccupancy(1, 100);
    CHECK(near(g.estimatedCost(0, e), 10 * (1 + 0.5 * 1.0)));
    CHECK(near(g.estimatedCost(1, g.getEdges(1)[0]), 15.0));
    CHECK(near(g.estimatedCost(0, e, true), 20 * (1 + 0.5 * 1.0)));

    g.updateOccupancy(1, 200);
    CHECK(near(g.estimatedCost(0, e), 10 * (1 + 0.5 * 2.0)));

    CHECK(g.updateCongestionLikelihood(0, 1, 0));
    CHECK(near(g.estimatedCost(0, g.getEdges(0)[0]), 10.0));
    CHECK(!g.updateCongestionLikelihood(0, 1, 101));
    CHECK(!g.updateCongestionLikelihood(0, 1, -1));
    CHECK(!g.updateCongestionLikelihood(0, 5, 10));
}

static void testCongestionChangesRoute() {
    std::cout << "[congestion changes the chosen route]\n";
    VenueGraph g = makeDiamond();
    g.updateCongestionLikelihood(0, 1, 100);
    g.updateCongestionLikelihood(1, 3, 100);
    auto r = g.dijkstra(0);
    CHECK((pathTo(0, 3, r.parent) == std::vector<int>{0, 1, 3}));

    g.updateOccupancy(1, 100);
    r = g.dijkstra(0);
    CHECK((pathTo(0, 3, r.parent) == std::vector<int>{0, 2, 3}));
    CHECK(near(r.distance[3], 35));
}

static void testBFS() {
    std::cout << "[bfs]\n";
    VenueGraph g = makeDiamond();
    g.addLocation("Island", "Hall", 10, 0, true);
    auto order = g.reachableByBFS(0);
    CHECK(order.size() == 4);
    CHECK(order[0] == 0);
    for (int id : order) CHECK(id != 4);

    g.setLocationOpen(1, false);
    g.setLocationOpen(2, false);
    order = g.reachableByBFS(0);
    CHECK(order.size() == 1 && order[0] == 0);

    CHECK(g.reachableByBFS(1).empty());
    CHECK(g.reachableByBFS(-1).empty());

    g.setLocationOpen(1, true);
    g.setLocationOpen(2, true);
    g.setConnectionOpen(1, 3, false);
    g.setConnectionOpen(2, 3, false);
    order = g.reachableByBFS(0);
    CHECK(order.size() == 3);
}

static void testEvacuationMultiExit() {
    std::cout << "[evacuation multi-exit]\n";
    VenueGraph g;
    g.addLocation("ExitL", "Exit", 100, 0, true);
    g.addLocation("Mid", "Hall", 100, 0, true);
    g.addLocation("ExitR", "Exit", 100, 0, true);
    g.addLocation("Far", "Hall", 100, 0, true);
    g.addConnection(0, 1, 4, 4, 0, true);
    g.addConnection(1, 2, 6, 6, 0, true);
    g.addConnection(1, 3, 1, 1, 0, true);

    auto e = g.multiSourceEvacuation();
    CHECK(near(e.distance[0], 0) && near(e.distance[2], 0));
    CHECK(near(e.distance[1], 4) && e.nextTowardExit[1] == 0);
    CHECK(near(e.distance[3], 5) && e.nextTowardExit[3] == 1);

    VenueGraph h;
    h.addLocation("Ex", "Exit", 10, 0, true);
    h.addLocation("A", "Hall", 10, 0, true);
    h.addConnection(0, 1, 5, 50, 0, true);
    CHECK(near(h.multiSourceEvacuation().distance[1], 50));
}

static void testEvacuationClosedExit() {
    std::cout << "[evacuation closed exit -> reroute]\n";
    VenueGraph g;
    g.addLocation("ExitL", "Exit", 100, 0, true);
    g.addLocation("Mid", "Hall", 100, 0, true);
    g.addLocation("ExitR", "Exit", 100, 0, true);
    g.addConnection(0, 1, 4, 4, 0, true);
    g.addConnection(1, 2, 6, 6, 0, true);

    g.setLocationOpen(0, false);
    auto e = g.multiSourceEvacuation();
    CHECK(near(e.distance[1], 6) && e.nextTowardExit[1] == 2);
    CHECK(e.distance[0] == INF);
}

static void testEvacuationAllExitsClosed() {
    std::cout << "[evacuation all exits closed / none exist]\n";
    VenueGraph g = makeDiamond();
    g.setLocationOpen(3, false);
    auto e = g.multiSourceEvacuation();
    for (int i = 0; i < g.size(); ++i) CHECK(e.distance[static_cast<std::size_t>(i)] == INF);

    VenueGraph none;
    none.addLocation("A", "Hall", 10, 0, true);
    none.addLocation("B", "Hall", 10, 0, true);
    none.addConnection(0, 1, 1, 1, 0, true);
    auto e2 = none.multiSourceEvacuation();
    CHECK(e2.distance[0] == INF && e2.distance[1] == INF);

    VenueGraph empty;
    CHECK(empty.multiSourceEvacuation().distance.empty());
}

static void testEvacuationDisconnectedAndClosedEdge() {
    std::cout << "[evacuation disconnected / closed edge]\n";
    VenueGraph g = makeDiamond();
    g.addLocation("Island", "Hall", 10, 0, true);
    auto e = g.multiSourceEvacuation();
    CHECK(e.distance[4] == INF && e.nextTowardExit[4] == -1);
    CHECK(near(e.distance[1], 10));

    g.setConnectionOpen(1, 3, false);
    g.setConnectionOpen(2, 3, false);
    e = g.multiSourceEvacuation();
    CHECK(e.distance[0] == INF && e.distance[1] == INF && e.distance[2] == INF);
}

static void testEvacuationFollowsPointers() {
    std::cout << "[evacuation nextTowardExit chain ends at an exit]\n";
    VenueGraph g = makeDiamond();
    auto e = g.multiSourceEvacuation();
    int cur = 0;
    int steps = 0;
    while (g.getLocation(cur).type != "Exit" && steps++ < g.size()) {
        cur = e.nextTowardExit[static_cast<std::size_t>(cur)];
        CHECK(cur != -1);
        if (cur == -1) return;
    }
    CHECK(g.getLocation(cur).type == "Exit");
}

static void testHysteresis() {
    std::cout << "[congestion hysteresis 80/70]\n";
    VenueGraph g;
    g.addLocation("Hall", "Hall", 100, 0, true);
    CHECK(!g.getLocation(0).congestionAlertActive);

    g.updateOccupancy(0, 79);
    CHECK(!g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 80);
    CHECK(g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 75);
    CHECK(g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 71);
    CHECK(g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 70);
    CHECK(!g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 79);
    CHECK(!g.getLocation(0).congestionAlertActive);
    g.updateOccupancy(0, 150);
    CHECK(g.getLocation(0).congestionAlertActive);
    CHECK(g.getLocation(0).occupancy == 150);

    CHECK(!g.updateOccupancy(0, -1));
    CHECK(!g.updateOccupancy(7, 10));
    CHECK(g.getLocation(0).occupancy == 150);

    VenueGraph h;
    h.addLocation("Full", "Hall", 10, 9, true);
    CHECK(h.getLocation(0).congestionAlertActive);
}

static void testConnectionToggle() {
    std::cout << "[connection open/close is symmetric]\n";
    VenueGraph g = makeDiamond();
    CHECK(g.setConnectionOpen(0, 1, false));
    CHECK(!g.getEdges(0)[0].open);
    bool backClosed = false;
    for (const auto& e : g.getEdges(1)) if (e.to == 0) backClosed = !e.open;
    CHECK(backClosed);
    CHECK(!g.setConnectionOpen(0, 3, false));
    CHECK(!g.setLocationOpen(42, false));
}

int main() {
    testBuildAndValidation();
    testConfigFallback();
    testDijkstraBasic();
    testDijkstraDisconnected();
    testDijkstraClosedNodes();
    testCongestionCost();
    testCongestionChangesRoute();
    testBFS();
    testEvacuationMultiExit();
    testEvacuationClosedExit();
    testEvacuationAllExitsClosed();
    testEvacuationDisconnectedAndClosedEdge();
    testEvacuationFollowsPointers();
    testHysteresis();
    testConnectionToggle();

    std::cout << "\nPassed: " << g_passed << "   Failed: " << g_failed << '\n';
    return g_failed == 0 ? 0 : 1;
}
