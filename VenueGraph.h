#ifndef CAMPUSFLOW_VENUE_GRAPH_H
#define CAMPUSFLOW_VENUE_GRAPH_H

#include "Config.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace campusflow {

namespace detail {
inline std::string trimGraphText(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

inline std::string normalizeLocationName(const std::string& name) {
    std::string result = trimGraphText(name);
    std::transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

inline constexpr double EPSILON = 1e-9;
} // namespace detail

class VenueGraph {
public:
    struct Location {
        std::string name;
        std::string type;
        int capacity = 1;
        int occupancy = 0;
        bool open = true;
        bool congestionAlertActive = false;
    };

    struct Edge {
        int to = -1;
        double normalTimeSeconds = 0.0;
        double clearanceTimeSeconds = 0.0;
        double congestionLikelihoodPercent = 0.0;
        bool open = true;
    };

    struct SearchResult {
        std::vector<double> distance;
        std::vector<int> parent;
    };

    struct EvacuationResult {
        std::vector<double> distance;
        // For each location, nextTowardExit points to the next location on
        // its best route toward an available exit.
        //it is reqired to find the next location on the path to an exit
        std::vector<int> nextTowardExit;
    };

private:
    Config config;
    std::vector<Location> locations;
    std::vector<std::vector<Edge>> adjacency;
    std::unordered_map<std::string, int> idByName;

    double occupancyRatio(int locationId) const {
        return static_cast<double>(locations[locationId].occupancy) /
               locations[locationId].capacity;
    }

    void refreshCongestionAlert(int locationId) {
        Location& location = locations[locationId];
        const double ratio = occupancyRatio(locationId);
        //alert is on when the occ ratio is 80%
        //these are defined in the config file
        if (!location.congestionAlertActive &&
            ratio >= config.highCongestionThresholdRatio) {
            location.congestionAlertActive = true;
        } else if (location.congestionAlertActive &&
                   ratio <= config.clearCongestionThresholdRatio) {
            location.congestionAlertActive = false;
        }
    }

    const Edge* findEdge(int from, int to) const {
        if (!validId(from) || !validId(to)) return nullptr;
        for (const Edge& edge : adjacency[from]) {
            if (edge.to == to) return &edge;
        }
        return nullptr;
    }

    double effectiveCost(int from, const Edge& edge,
                         bool evacuationMode) const {
        const double baseTime = evacuationMode
                                    ? edge.clearanceTimeSeconds
                                    : edge.normalTimeSeconds;

        // The larger endpoint occupancy ratio is a simple proxy for local
        // congestion. It is a project heuristic, not a measured prediction.
        const double localCongestion =
            std::max(occupancyRatio(from), occupancyRatio(edge.to));
        const double likelihood = edge.congestionLikelihoodPercent / 100.0;

        return baseTime * (1.0 + likelihood * localCongestion);
    }

public:
    explicit VenueGraph(const Config& settings = Config{}) : config(settings) {
        if (!std::isfinite(config.clearCongestionThresholdRatio) ||
            !std::isfinite(config.highCongestionThresholdRatio) ||
            config.clearCongestionThresholdRatio < 0.0 ||
            config.clearCongestionThresholdRatio > 1.0 ||
            config.highCongestionThresholdRatio < 0.0 ||
            config.highCongestionThresholdRatio > 1.0 ||
            config.clearCongestionThresholdRatio >=
                config.highCongestionThresholdRatio) {
            config.clearCongestionThresholdRatio = 0.70;
            config.highCongestionThresholdRatio = 0.80;
        }
    }

    int size() const {
        return static_cast<int>(locations.size());
    }

    bool validId(int id) const {
        return id >= 0 && id < size();
    }

    int findLocation(const std::string& name) const {
        const auto it = idByName.find(detail::normalizeLocationName(name));
        return it == idByName.end() ? -1 : it->second;
    }

    const Location& getLocation(int id) const {
        return locations.at(id);
    }

    const std::vector<Edge>& getEdges(int id) const {
        return adjacency.at(id);
    }

    bool addLocation(const std::string& name, const std::string& type,
                     int capacity, int initialOccupancy, bool open) {
        const std::string cleanName = detail::trimGraphText(name);
        const std::string key = detail::normalizeLocationName(cleanName);
        if (cleanName.empty() || idByName.count(key) != 0 || capacity <= 0 ||
            initialOccupancy < 0) {
            return false;
        }

        const int id = size();
        locations.push_back(
            {cleanName, type, capacity, initialOccupancy, open, false});
        adjacency.emplace_back();
        idByName[key] = id;
        refreshCongestionAlert(id);
        return true;
    }

    bool hasConnection(int u, int v) const {
        return findEdge(u, v) != nullptr || findEdge(v, u) != nullptr;
    }

    bool addConnection(int u, int v, double normalTimeSeconds,
                       double clearanceTimeSeconds,
                       double congestionLikelihoodPercent, bool open) {
        if (!validId(u) || !validId(v) || u == v || hasConnection(u, v) ||
            normalTimeSeconds <= 0.0 ||
            clearanceTimeSeconds <= 0.0 ||
            congestionLikelihoodPercent < 0.0 ||
            congestionLikelihoodPercent > 100.0) {
            return false;
        }
        //bidirectional connection: add to both adjacency lists
        adjacency[u].push_back({v, normalTimeSeconds, clearanceTimeSeconds,
                                congestionLikelihoodPercent, open});
        adjacency[v].push_back({u, normalTimeSeconds, clearanceTimeSeconds,
                                congestionLikelihoodPercent, open});
        return true;
    }

    bool updateOccupancy(int id, int people) {
        if (!validId(id) || people < 0) return false;
        locations[id].occupancy = people;
        refreshCongestionAlert(id);
        return true;
    }

    bool setLocationOpen(int id, bool open) {
        if (!validId(id)) return false;
        locations[id].open = open;
        return true;
    }

    bool setConnectionOpen(int u, int v, bool open) {
        if (!validId(u) || !validId(v)) return false;
        bool changed = false;
        for (Edge& edge : adjacency[u]) {
            if (edge.to == v) {
                edge.open = open;
                changed = true;
            }
        }
        for (Edge& edge : adjacency[v]) {
            if (edge.to == u) {
                edge.open = open;
                changed = true;
            }
        }
        return changed;
    }

    bool updateCongestionLikelihood(int u, int v, double percentage) {
        if (!validId(u) || !validId(v) || percentage < 0.0 ||
            percentage > 100.0) {
            return false;
        }
        bool changed = false;
        for (Edge& edge : adjacency[u]) {
            if (edge.to == v) {
                edge.congestionLikelihoodPercent = percentage;
                changed = true;
            }
        }
        for (Edge& edge : adjacency[v]) {
            if (edge.to == u) {
                edge.congestionLikelihoodPercent = percentage;
                changed = true;
            }
        }
        return changed;
    }

    double estimatedCost(int from, const Edge& edge,
                         bool evacuationMode = false) const {
        return effectiveCost(from, edge, evacuationMode);
    }

    SearchResult dijkstra(int source, bool evacuationMode = false) const {
        const double INF = std::numeric_limits<double>::infinity();
        SearchResult result{std::vector<double>(size(), INF),
                            std::vector<int>(size(), -1)};
        if (!validId(source) || !locations[source].open) return result;

        using QueueEntry = std::pair<double, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;
        result.distance[source] = 0.0;
        pq.push({0.0, source});

        while (!pq.empty()) {
            const auto [currentDistance, u] = pq.top();
            pq.pop();

            if (currentDistance > result.distance[u] + detail::EPSILON) continue;
            if (!locations[u].open) continue;

            for (const Edge& edge : adjacency[u]) {
                const int v = edge.to;
                if (!edge.open || !locations[v].open) continue;

                const double candidate = currentDistance +
                    effectiveCost(u, edge, evacuationMode);
                if (candidate + detail::EPSILON < result.distance[v]) {
                    result.distance[v] = candidate;
                    result.parent[v] = u;
                    pq.push({candidate, v});
                }
            }
        }
        return result;
    }

    std::vector<int> reachableByBFS(int source) const {
        std::vector<int> order;
        if (!validId(source) || !locations[source].open) return order;

        std::vector<bool> visited(size(), false);
        std::queue<int> q;
        visited[source] = true;
        q.push(source);

        while (!q.empty()) {
            const int u = q.front();
            q.pop();
            order.push_back(u);

            for (const Edge& edge : adjacency[u]) {
                const int v = edge.to;
                if (edge.open && locations[v].open && !visited[v]) {
                    visited[v] = true;
                    q.push(v);
                }
            }
        }
        return order;
    }

    EvacuationResult multiSourceEvacuation() const {
        const double INF = std::numeric_limits<double>::infinity();
        EvacuationResult result{std::vector<double>(size(), INF),
                                std::vector<int>(size(), -1)};
        using QueueEntry = std::pair<double, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;

        // Seed Dijkstra with every open exit at once.
        for (int i = 0; i < size(); ++i) {
            if (locations[i].open && locations[i].type == "Exit") {
                result.distance[i] = 0.0;
                pq.push({0.0, i});
            }
        }

        while (!pq.empty()) {
            const auto [currentDistance, u] = pq.top();
            pq.pop();
            if (currentDistance > result.distance[u] + detail::EPSILON) continue;

            for (const Edge& edge : adjacency[u]) {
                const int v = edge.to;
                if (!edge.open || !locations[v].open) continue;

                // Connections are bidirectional, so expanding outward from
                // exits computes the best route from each location to an exit.
                const double candidate = currentDistance +
                    effectiveCost(u, edge, true);
                if (candidate + detail::EPSILON < result.distance[v]) {
                    result.distance[v] = candidate;
                    result.nextTowardExit[v] = u;
                    pq.push({candidate, v});
                }
            }
        }
        return result;
    }

    void displayVenue() const {
        std::cout << "\n========== VENUE LOCATIONS ==========\n";
        if (locations.empty()) {
            std::cout << "No locations have been entered.\n";
            return;
        }

        std::cout << std::fixed << std::setprecision(1);
        for (int i = 0; i < size(); ++i) {
            const Location& loc = locations[i];
            std::cout << "[" << i << "] " << loc.name << " (" << loc.type << ")"
                 << " | occupancy " << loc.occupancy << "/" << loc.capacity
                 << " (" << occupancyRatio(i) * 100.0 << "%)"
                 << " | " << (loc.open ? "ACCESSIBLE" : "CLOSED")
                 << " | congestion alert: "
                 << (loc.congestionAlertActive ? "ACTIVE" : "off") << '\n';
        }

        std::cout << "\n========== CONNECTIONS (BIDIRECTIONAL) ==========\n";
        bool anyConnection = false;
        for (int u = 0; u < size(); ++u) {
            for (const Edge& edge : adjacency[u]) {
                if (edge.to <= u) continue; // Print each corridor once.
                anyConnection = true;
                std::cout << locations[u].name << " <-> "
                     << locations[edge.to].name
                     << " | normal " << edge.normalTimeSeconds << " s"
                     << " | clearance " << edge.clearanceTimeSeconds << " s"
                     << " | congestion likelihood "
                     << edge.congestionLikelihoodPercent << "%"
                     << " | " << (edge.open ? "OPEN" : "CLOSED") << '\n';
            }
        }
        if (!anyConnection) std::cout << "No connections have been entered.\n";
    }

    void printAnalyticsReport() const {
        int openLocations = 0;
        int exitCount = 0;
        int activeAlerts = 0;
        int overCapacity = 0;
        int openConnections = 0;
        int closedConnections = 0;
        int totalConnections = 0;

        std::cout << "\n========== CAMPUSFLOW CROWD REPORT ==========\n";
        std::cout << std::fixed << std::setprecision(2);
        for (int i = 0; i < size(); ++i) {
            const Location& loc = locations[i];
            if (loc.open) ++openLocations;
            if (loc.open && loc.type == "Exit") ++exitCount;
            if (loc.congestionAlertActive) ++activeAlerts;
            if (loc.occupancy > loc.capacity) ++overCapacity;

            std::cout << loc.name << ": " << loc.occupancy << "/" << loc.capacity
                 << " people (" << occupancyRatio(i) * 100.0 << "%)"
                 << ", " << (loc.open ? "accessible" : "CLOSED")
                 << ", alert="
                 << (loc.congestionAlertActive ? "ACTIVE" : "off") << '\n';
        }

        for (int u = 0; u < size(); ++u) {
            for (const Edge& edge : adjacency[u]) {
                if (edge.to <= u) continue;
                ++totalConnections;
                if (edge.open && locations[u].open && locations[edge.to].open)
                    ++openConnections;
                else
                    ++closedConnections;
            }
        }

        std::cout << "\nSummary\n"
             << "Locations: " << size() << '\n'
             << "Accessible locations: " << openLocations << '\n'
             << "Available exits: " << exitCount << '\n'
             << "Active congestion alerts: " << activeAlerts << '\n'
             << "Locations over capacity: " << overCapacity << '\n'
             << "Connections: " << totalConnections << '\n'
             << "Usable connections: " << openConnections << '\n'
             << "Closed/unusable connections: " << closedConnections << '\n';

        if (exitCount == 0) {
            std::cout << "WARNING: no accessible exits are configured.\n";
        }
        if (activeAlerts > 0) {
            std::cout << "WARNING: review locations with active congestion alerts.\n";
        }
        std::cout << "Note: route costs are estimates based on entered values and the "
                "project's congestion heuristic; they are not live sensor data.\n";
    }
};

} // namespace campusflow

#endif // CAMPUSFLOW_VENUE_GRAPH_H
