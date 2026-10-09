#pragma once

#include "RouteMap.generated.h"
#include "Types.h"
#include <cstring>

// Topological turns at each RFID. A route alone cannot establish the physical
// heading of a robot placed on its first tag; Unknown must not authorize motion.
enum class RouteTurn { Unknown, Straight, Left, Right, Reverse };
struct RoutePlan {
    RouteTurn departure[Config::MAX_PATH_NODES]{};
    uint8_t count = 0;
};

namespace RoutePlanner {
inline const char* turnName(RouteTurn turn) {
    switch (turn) {
        case RouteTurn::Unknown: return "UNKNOWN_HEADING";
        case RouteTurn::Straight: return "STRAIGHT";
        case RouteTurn::Left: return "LEFT";
        case RouteTurn::Right: return "RIGHT";
        case RouteTurn::Reverse: return "REVERSE";
    }
    return "UNKNOWN_HEADING";
}

inline const RouteMap::Node* node(const char* id) {
    if (!id) return nullptr;
    for (const auto& n : RouteMap::NODES)
        if (std::strcmp(n.id, id) == 0) return &n;
    return nullptr;
}

inline bool directedEdge(const char* from, const char* to) {
    if (!node(from) || !node(to)) return false;
    for (const auto& edge : RouteMap::EDGES) {
        if (std::strcmp(edge.from, from) == 0 && std::strcmp(edge.to, to) == 0) return true;
        if (edge.twoWay && std::strcmp(edge.to, from) == 0 && std::strcmp(edge.from, to) == 0) return true;
    }
    return false;
}

// Supervised one-robot trial: only robot 01, with a fully resolved directed
// path. The server independently limits control to this one robot.
inline bool oneRobotTrialRoute(const char* robotId, const char* current,
                               const CoreCommand& command, const RoutePlan& route) {
    return robotId && current && std::strcmp(robotId, "01") == 0 &&
           command.type == CoreCommandType::Goto && command.pathLength > 0 &&
           command.pathLength <= Config::MAX_PATH_NODES &&
           command.pathLength == route.count &&
           std::strcmp(command.path[command.pathLength - 1], command.target) == 0 &&
           [&] {
               const char* from = current;
               for (uint8_t i = 0; i < route.count; ++i) {
                   if (!directedEdge(from, command.path[i]) ||
                       route.departure[i] == RouteTurn::Unknown) return false;
                   from = command.path[i];
               }
               return true;
           }();
}

inline RouteTurn turn(const char* previous, const char* current, const char* next) {
    const auto* a = node(previous);
    const auto* b = node(current);
    const auto* c = node(next);
    if (!a || !b || !c || !directedEdge(previous, current) || !directedEdge(current, next))
        return RouteTurn::Unknown;
    if (std::strcmp(previous, next) == 0) return RouteTurn::Reverse;
    const int64_t inX = b->x - a->x, inY = b->y - a->y;
    const int64_t outX = c->x - b->x, outY = c->y - b->y;
    const int64_t cross = inX * outY - inY * outX;
    const int64_t dot = inX * outX + inY * outY;
    // Map y increases downwards, so positive cross is a physical right turn.
    if (cross > 0) return RouteTurn::Right;
    if (cross < 0) return RouteTurn::Left;
    return dot > 0 ? RouteTurn::Straight : RouteTurn::Unknown;
}

inline bool plan(const char* previous, const char* current, const CoreCommand& command,
                 RoutePlan& output, bool previousWasReverse = false) {
    output = RoutePlan{};
    if (command.type != CoreCommandType::Goto || !node(current) || !node(command.target) ||
        command.pathLength > Config::MAX_PATH_NODES) return false;
    if (command.pathLength == 0) return std::strcmp(current, command.target) == 0;
    if (std::strcmp(command.path[command.pathLength - 1], command.target) != 0) return false;
    const char* from = current;
    for (uint8_t i = 0; i < command.pathLength; ++i) {
        const char* to = command.path[i];
        if (!directedEdge(from, to)) return false;
        const char* incoming = i == 0 ? previous : (i == 1 ? current : command.path[i - 2]);
        RouteTurn decision = incoming && incoming[0] ? turn(incoming, from, to) : RouteTurn::Unknown;
        // The operator confirmed each vehicle faces its own GS tag while at S1/S2.
        // Only these two boot headings are known without a previous RFID.
        if (i == 0 && (!incoming || !incoming[0]) &&
            ((std::strcmp(from, "S1") == 0 && std::strcmp(to, "GS1") == 0) ||
             (std::strcmp(from, "S2") == 0 && std::strcmp(to, "GS2") == 0)))
            decision = RouteTurn::Straight;
        if (previousWasReverse) {
            if (decision == RouteTurn::Left) decision = RouteTurn::Right;
            else if (decision == RouteTurn::Right) decision = RouteTurn::Left;
            else if (decision == RouteTurn::Reverse) decision = RouteTurn::Straight;
            else if (decision == RouteTurn::Straight) decision = RouteTurn::Reverse; // continue backing on the same line
        }
        output.departure[i] = decision;
        previousWasReverse = decision == RouteTurn::Reverse;
        // A claimed previous RFID that is inconsistent with the one-way graph is unsafe.
        if (incoming && incoming[0] && output.departure[i] == RouteTurn::Unknown) return false;
        from = to;
    }
    output.count = command.pathLength;
    return true;
}
}
