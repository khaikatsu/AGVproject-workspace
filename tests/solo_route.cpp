#include "map/MapGraph.hpp"
#include "../firmware/NCKHAGV/NCKHAGV/src/core/RoutePlanner.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

static CoreCommand commandFor(const std::vector<std::string>& path) {
    if (path.size() < 2 || path.size() - 1 > Config::MAX_PATH_NODES)
        throw std::runtime_error("Path does not fit firmware command");
    CoreCommand command;
    command.type = CoreCommandType::Goto;
    command.pathLength = static_cast<uint8_t>(path.size() - 1);
    for (const auto& node : path)
        if (node.size() >= NODE_TEXT_SIZE) throw std::runtime_error("Node ID does not fit firmware");
    std::strcpy(command.target, path.back().c_str());
    for (size_t i = 1; i < path.size(); ++i)
        std::strcpy(command.path[i - 1], path[i].c_str());
    return command;
}

int main() {
    try {
        MapGraph map;
        map.loadFromFile("data/map_layout.json");
        const auto nodes = map.toJson().at("nodes");
        int checked = 0;
        for (const auto& startNode : nodes) {
            const std::string start = startNode.at("id").get<std::string>();
            const auto arrival = map.findShortestPath("S1", start);
            if (arrival.empty()) throw std::runtime_error("S1 cannot reach " + start);
            std::string previous;
            bool previousReverse = false;
            if (start != "S1") {
                const auto first = commandFor(arrival);
                RoutePlan firstPlan;
                if (!RoutePlanner::plan(nullptr, "S1", first, firstPlan) ||
                    !RoutePlanner::oneRobotTrialRoute("01", "S1", first, firstPlan))
                    throw std::runtime_error("Firmware cannot follow S1 -> " + start);
                previous = arrival[arrival.size() - 2];
                previousReverse = firstPlan.departure[firstPlan.count - 1] == RouteTurn::Reverse;
            }
            for (const auto& targetNode : nodes) {
                const std::string target = targetNode.at("id").get<std::string>();
                if (start == target) continue;
                const auto path = map.findShortestPath(start, target);
                if (path.empty()) throw std::runtime_error(start + " cannot reach " + target);
                const auto command = commandFor(path);
                RoutePlan route;
                if (!RoutePlanner::plan(previous.empty() ? nullptr : previous.c_str(),
                                        start.c_str(), command, route, previousReverse) ||
                    !RoutePlanner::oneRobotTrialRoute("01", start.c_str(), command, route))
                    throw std::runtime_error("Firmware cannot follow " + start + " -> " + target +
                                             " after arriving from " + previous);
                ++checked;
            }
        }
        std::cout << "PASS solo directed Dijkstra and firmware route for " << checked
                  << " start/target pairs\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
