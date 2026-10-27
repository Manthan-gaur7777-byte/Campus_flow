#include "VenueGraph.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using campusflow::VenueGraph;
namespace {
string trim(const string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string normalizedName(const string& name) {
    string result = trim(name);
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return result;
}

int readInt(const string& prompt, int minimum, int maximum) {
    while (true) {
        cout << prompt;
        int value;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (value >= minimum && value <= maximum) return value;
        } else {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        cout << "Invalid input. Enter an integer from " << minimum
             << " to " << maximum << ".\n";
    }
}

double readDouble(const string& prompt, double minimum, double maximum) {
    while (true) {
        cout << prompt;
        double value;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (isfinite(value) && value >= minimum && value <= maximum)
                return value;
        } else {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        cout << "Invalid input. Enter a number from " << minimum
             << " to " << maximum << ".\n";
    }
}

string readNonEmptyLine(const string& prompt) {
    while (true) {
        cout << prompt;
        string value;
        getline(cin, value);
        value = trim(value);
        if (!value.empty()) return value;
        cout << "The value cannot be empty. Try again.\n";
    }
}

bool readYesNo(const string& prompt) {
    while (true) {
        cout << prompt << " (y/n): ";
        string value;
        getline(cin, value);
        value = normalizedName(value);
        if (value == "y" || value == "yes") return true;
        if (value == "n" || value == "no") return false;
        cout << "Please enter y or n.\n";
    }
}
} // namespace

namespace {
string chooseLocationType() {
    cout << "Location type:\n"
         << "  1. Entrance\n"
         << "  2. Hall\n"
         << "  3. Junction\n"
         << "  4. Exit\n"
         << "  5. Other\n";
    switch (readInt("Choose type: ", 1, 5)) {
        case 1: return "Entrance";
        case 2: return "Hall";
        case 3: return "Junction";
        case 4: return "Exit";
        default: return "Other";
    }
}

int promptKnownLocation(const VenueGraph& graph, const string& prompt) {
    while (true) {
        const string name = readNonEmptyLine(prompt);
        const int id = graph.findLocation(name);
        if (id != -1) return id;
        cout << "Location not found. Check the name and try again.\n";
    }
}

void setupVenue(VenueGraph& graph) {
    cout << "\n========== INITIAL VENUE SETUP ==========\n"
         << "Enter the venue locations first, then their connections.\n"
         << "Location names are case-insensitive when searched.\n"
         << "Occupancy may exceed capacity so overloads can be reported.\n";

    const int locationCount = readInt("Number of locations (1-500): ", 1, 500);
    for (int i = 0; i < locationCount; ++i) {
        cout << "\nLocation " << (i + 1) << " of " << locationCount << '\n';
        while (true) {
            const string name = readNonEmptyLine("Location name: ");
            if (graph.findLocation(name) != -1) {
                cout << "That name already exists. Use a unique name.\n";
                continue;
            }
            const string type = chooseLocationType();
            const int capacity = readInt("Maximum capacity (people, > 0): ", 1, 1000000);
            const int occupancy = readInt("Initial occupancy (people, >= 0): ", 0, 10000000);
            const bool open = readYesNo("Is this location currently accessible?");
            if (graph.addLocation(name, type, capacity, occupancy, open)) {
                break;
            }
            cout << "Could not add that location. Please try again.\n";
        }
    }

    const int maxConnections = locationCount * (locationCount - 1) / 2;
    const int connectionCount = readInt(
        "Number of connections/corridors (0-" + to_string(maxConnections) + "): ",
        0, maxConnections);

    for (int i = 0; i < connectionCount; ++i) {
        cout << "\nConnection " << (i + 1) << " of " << connectionCount << '\n';
        int u, v;
        while (true) {
            u = promptKnownLocation(graph, "From location name: ");
            v = promptKnownLocation(graph, "To location name: ");
            if (u == v) {
                cout << "A connection must join two different locations.\n";
            } else if (graph.hasConnection(u, v)) {
                cout << "That connection already exists. Enter a different pair.\n";
            } else {
                break;
            }
        }

        const double normalTime = readDouble(
            "Estimated normal one-way travel time in seconds (> 0 and the minimum value is 0.001): ",
            0.001, 1000000.0);
        const double clearanceTime = readDouble(
            "Estimated crowd clearance time for this connection in seconds (> 0 and the minimum value is 0.001): ",
            0.001, 10000000.0);
        const double likelihood = readDouble(
            "How prone is this connection to congestion? Enter 0-100 (%): ",
            0.0, 100.0);
        const bool open = readYesNo("Is this connection currently open?");

        if (!graph.addConnection(u, v, normalTime, clearanceTime,
                                 likelihood, open)) {
            cout << "Could not add the connection. Please check its values.\n";
            --i;
        }
    }
    cout << "\nVenue setup complete. The main menu is ready.\n";
}

vector<int> reconstructPath(int source, int target,
                            const vector<int>& parent, int vertexCount) {
    vector<int> path;
    if (source < 0 || target < 0 || source >= vertexCount ||
        target >= vertexCount) return path;

    int current = target;
    for (int steps = 0; current != -1 && steps <= vertexCount; ++steps) {
        path.push_back(current);
        if (current == source) break;
        current = parent[current];
    }
    if (path.empty() || path.back() != source) return {};
    reverse(path.begin(), path.end());
    return path;
}

void displayPath(const VenueGraph& graph, const vector<int>& path,
                 double totalCost, bool evacuationMode) {
    if (path.empty()) {
        cout << "No route is available.\n";
        return;
    }

    cout << fixed << setprecision(2);
    cout << "Route: ";
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) cout << " -> ";
        cout << graph.getLocation(path[i]).name;
    }
    cout << "\nTotal estimated "
         << (evacuationMode ? "evacuation route cost" : "travel time")
         << ": " << totalCost << " seconds\n";

    for (size_t i = 1; i < path.size(); ++i) {
        const int from = path[i - 1];
        const int to = path[i];
        const VenueGraph::Edge* selectedEdge = nullptr;
        for (const auto& edge : graph.getEdges(from)) {
            if (edge.to == to) {
                selectedEdge = &edge;
                break;
            }
        }
        if (selectedEdge != nullptr) {
            cout << "  " << graph.getLocation(from).name << " -> "
                 << graph.getLocation(to).name
                 << " | base="
                 << (evacuationMode ? selectedEdge->clearanceTimeSeconds
                                    : selectedEdge->normalTimeSeconds)
                 << " s | adjusted="
                 << graph.estimatedCost(from, *selectedEdge, evacuationMode)
                 << " s\n";
        }
    }
}

void findFastestRoute(const VenueGraph& graph) {
    const int source = promptKnownLocation(graph, "Start location: ");
    const int target = promptKnownLocation(graph, "Destination location: ");
    if (!graph.getLocation(source).open || !graph.getLocation(target).open) {
        cout << "The start or destination is closed/inaccessible.\n";
        return;
    }

    const auto result = graph.dijkstra(source, false);
    if (!isfinite(result.distance[target])) {
        cout << "No open route connects those locations.\n";
        return;
    }
    displayPath(graph,
                reconstructPath(source, target, result.parent, graph.size()),
                result.distance[target], false);
}

void showReachableLocations(const VenueGraph& graph) {
    const int source = promptKnownLocation(graph, "Starting location: ");
    const vector<int> reachable = graph.reachableByBFS(source);
    if (reachable.empty()) {
        cout << "The starting location is closed/inaccessible.\n";
        return;
    }
    cout << "Reachable locations from " << graph.getLocation(source).name
         << " (BFS order):\n";
    for (const int id : reachable) cout << "  - " << graph.getLocation(id).name << '\n';
    cout << "Total reachable locations, including the start: "
         << reachable.size() << '\n';
}

int findExitAtEnd(const VenueGraph& graph, const VenueGraph::EvacuationResult& result,
                  int start) {
    int current = start;
    for (int steps = 0; graph.validId(current) && steps <= graph.size(); ++steps) {
        if (graph.getLocation(current).type == "Exit" &&
            graph.getLocation(current).open) return current;
        current = result.nextTowardExit[current];
    }
    return -1;
}

void findNearestExit(const VenueGraph& graph) {
    const int start = promptKnownLocation(graph, "Starting location: ");
    if (!graph.getLocation(start).open) {
        cout << "That location is closed/inaccessible.\n";
        return;
    }

    const auto result = graph.multiSourceEvacuation();
    if (!isfinite(result.distance[start])) {
        cout << "No open exit is reachable from this location.\n";
        return;
    }

    vector<int> path;
    int current = start;
    for (int steps = 0; graph.validId(current) && steps <= graph.size(); ++steps) {
        path.push_back(current);
        if (graph.getLocation(current).type == "Exit" &&
            graph.getLocation(current).open) break;
        current = result.nextTowardExit[current];
    }

    if (path.empty() || graph.getLocation(path.back()).type != "Exit" ||
        !graph.getLocation(path.back()).open) {
        cout << "Could not reconstruct an evacuation route.\n";
        return;
    }
    displayPath(graph, path, result.distance[start], true);
    cout << "The chosen exit is " << graph.getLocation(path.back()).name << ".\n";
}

void showAllEvacuationRecommendations(const VenueGraph& graph) {
    const auto result = graph.multiSourceEvacuation();
    bool hasOpenExit = false;
    for (int i = 0; i < graph.size(); ++i) {
        if (graph.getLocation(i).open && graph.getLocation(i).type == "Exit") {
            hasOpenExit = true;
            break;
        }
    }
    if (!hasOpenExit) {
        cout << "No accessible exits are configured. Cannot generate evacuation recommendations.\n";
        return;
    }

    cout << fixed << setprecision(2);
    cout << "\n========== EVACUATION RECOMMENDATIONS ==========\n";
    for (int i = 0; i < graph.size(); ++i) {
        const auto& location = graph.getLocation(i);
        if (!location.open) {
            cout << location.name << ": location is closed.\n";
            continue;
        }
        if (!isfinite(result.distance[i])) {
            cout << location.name << ": no available exit is reachable.\n";
            continue;
        }
        const int exitId = findExitAtEnd(graph, result, i);
        cout << location.name << " -> ";
        if (exitId == -1) {
            cout << "route unavailable\n";
        } else {
            cout << graph.getLocation(exitId).name
                 << " | estimated route cost: "
                 << result.distance[i] << " s\n";
        }
    }
}

void updateOccupancy(VenueGraph& graph) {
    const int id = promptKnownLocation(graph, "Location to update: ");
    const int people = readInt("Current occupancy (people, >= 0): ", 0, 10000000);
    graph.updateOccupancy(id, people);
    cout << "Occupancy updated for " << graph.getLocation(id).name << ".\n";
    const auto& location = graph.getLocation(id);
    cout << "Current ratio: " << fixed << setprecision(1)
         << (100.0 * location.occupancy / location.capacity) << "%\n";
    if (location.congestionAlertActive)
        cout << "CONGESTION ALERT: this location is above the alert's clearing threshold.\n";
    else
        cout << "No active congestion alert for this location.\n";
}

void updateLocationAccess(VenueGraph& graph) {
    const int id = promptKnownLocation(graph, "Location to update: ");
    const bool open = readYesNo("Should this location be accessible?");
    graph.setLocationOpen(id, open);
    cout << graph.getLocation(id).name << " is now "
         << (open ? "accessible" : "closed") << ".\n";
}

void updateConnectionAccess(VenueGraph& graph) {
    const int u = promptKnownLocation(graph, "First location: ");
    const int v = promptKnownLocation(graph, "Second location: ");
    if (!graph.hasConnection(u, v)) {
        cout << "No connection exists between those locations.\n";
        return;
    }
    const bool open = readYesNo("Should this connection be open?");
    graph.setConnectionOpen(u, v, open);
    cout << "Connection status updated.\n";
}

void updateConnectionLikelihood(VenueGraph& graph) {
    const int u = promptKnownLocation(graph, "First location: ");
    const int v = promptKnownLocation(graph, "Second location: ");
    if (!graph.hasConnection(u, v)) {
        cout << "No connection exists between those locations.\n";
        return;
    }
    const double likelihood = readDouble(
        "Updated congestion-proneness value (0-100%): ", 0.0, 100.0);
    graph.updateCongestionLikelihood(u, v, likelihood);
    cout << "Congestion-proneness value updated.\n";
}

void showMenu() {
    cout << "\n============= CAMPUSFLOW: venue digital twin =============\n"
         << "1. Display venue and connections\n"
         << "2. Update a location's occupancy\n"
         << "3. Mark a location accessible/closed\n"
         << "4. Open/close a connection\n"
         << "5. Update a connection's congestion likelihood\n"
         << "6. Find minimum estimated-travel-time route (Dijkstra)\n"
         << "7. List reachable locations (BFS)\n"
         << "8. Find best evacuation route to an available exit\n"
         << "9. Show evacuation recommendations for all locations\n"
         << "10. Show crowd/congestion analytics report\n"
         << "0. Exit\n";
}
} // namespace

int main() {
    cout << "CampusFlow - Venue Graph and Crowd Movement Module\n"
         << "This prototype uses manually entered venue data.\n";

    VenueGraph graph;
    setupVenue(graph);

    while (true) {
        showMenu();
        const int choice = readInt("Choose an option: ", 0, 10);
        switch (choice) {
            case 1:
                graph.displayVenue();
                break;
            case 2:
                updateOccupancy(graph);
                break;
            case 3:
                updateLocationAccess(graph);
                break;
            case 4:
                updateConnectionAccess(graph);
                break;
            case 5:
                updateConnectionLikelihood(graph);
                break;
            case 6:
                findFastestRoute(graph);
                break;
            case 7:
                showReachableLocations(graph);
                break;
            case 8:
                findNearestExit(graph);
                break;
            case 9:
                showAllEvacuationRecommendations(graph);
                break;
            case 10:
                graph.printAnalyticsReport();
                break;
            case 0:
                cout << "CampusFlow closed.\n";
                return 0;
        }
    }
}
