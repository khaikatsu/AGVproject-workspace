#pragma once

#include "RouteMap.generated.h"
#include "Types.h"
#include <cstring>

// Topological turns at each RFID. A route alone cannot establish the physical
// heading of a robot placed on its first tag; Unknown must not authorize motion.
enum class RouteTurn { Unknown, Straight, Left, Right, Reverse, TurnAround };
struct RoutePlan {
    RouteTurn departure[Config::MAX_PATH_NODES]{};
    uint8_t turnCrossings[Config::MAX_PATH_NODES]{};
    bool reverseAfterTurn[Config::MAX_PATH_NODES]{};
    uint8_t count = 0;
};

namespace RoutePlanner {
// Only an arrival on the active commanded segment proves the incoming heading.
// Moving/scanning tags by hand while stopped must clear the old heading.
inline const char* confirmedIncoming(bool expectedArrival, const char* from, const char* to) {
    return expectedArrival && from && from[0] && to && to[0] &&
           std::strcmp(from, to) != 0 ? from : nullptr;
}

inline const char* turnName(RouteTurn turn) {
    switch (turn) {
        case RouteTurn::Unknown: return "UNKNOWN_HEADING";
        case RouteTurn::Straight: return "STRAIGHT";
        case RouteTurn::Left: return "LEFT";
        case RouteTurn::Right: return "RIGHT";
        case RouteTurn::Reverse: return "REVERSE";
        case RouteTurn::TurnAround: return "TURN_AROUND_RIGHT";
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

inline RobotHeading edgeHeading(const char* from, const char* to, bool reverse = false, bool arrival = false) {
    const auto* a = node(from); const auto* b = node(to);
    if (!a || !b || !directedEdge(from, to)) return RobotHeading::Unknown;
    for (const auto& edge : RouteMap::EDGES) {
        int heading=0;
        if (std::strcmp(edge.from,from)==0 && std::strcmp(edge.to,to)==0)
            heading=arrival ? edge.arrival : edge.departure;
        else if (edge.twoWay && std::strcmp(edge.to,from)==0 && std::strcmp(edge.from,to)==0) {
            heading=arrival ? edge.departure : edge.arrival;
            if (heading) heading=(heading-1+2)%4+1;
        }
        if (heading) return static_cast<RobotHeading>(reverse ? (heading-1+2)%4+1 : heading);
    }
    return RobotHeading::Unknown;
}
inline RouteTurn turnFromHeading(RobotHeading incoming, RobotHeading outgoing) {
    if (incoming == RobotHeading::Unknown || outgoing == RobotHeading::Unknown) return RouteTurn::Unknown;
    const int delta = (static_cast<int>(outgoing) - static_cast<int>(incoming) + 4) % 4;
    return delta == 0 ? RouteTurn::Straight : delta == 1 ? RouteTurn::Right :
           delta == 2 ? RouteTurn::Reverse : RouteTurn::Left;
}

// Warehouses face inward for cargo service; charging stations face their gate
// so each new route can depart forward without a docking heading reset.
inline bool station(const char* id) {
    return id && (std::strcmp(id,"S1") == 0 || std::strcmp(id,"S2") == 0);
}
inline const char* bayGate(const char* id) {
    if (!id || !id[0] || !id[1] || id[2]) return nullptr;
    if (id[0] == 'K' && id[1] >= '1' && id[1] <= '6') {
        static constexpr const char* gates[] = {"G1","G2","G3","G4","G5","G6"};
        return gates[id[1]-'1'];
    }
    if (std::strcmp(id,"S1") == 0) return "GS1";
    if (std::strcmp(id,"S2") == 0) return "GS2";
    return nullptr;
}
inline RobotHeading bayHeading(const char* id) {
    const char* gate = bayGate(id);
    return gate ? (station(id) ? edgeHeading(id,gate) : edgeHeading(gate,id)) : RobotHeading::Unknown;
}
inline bool bayExit(const char* from, const char* to) {
    const char* gate = bayGate(from);
    return gate && to && std::strcmp(gate,to) == 0 && directedEdge(from,to);
}
inline bool dockingReverse(const char* from, const char* to) {
    return !station(from) && bayExit(from,to);
}
inline bool reverseHome(const char* from, const char* to) {
    return Config::REVERSE_HOME_SUPPORTED && from && to &&
        ((std::strcmp(from,"GS1") == 0 && std::strcmp(to,"S1") == 0) ||
         (std::strcmp(from,"GS2") == 0 && std::strcmp(to,"S2") == 0));
}
inline bool straightThrough(const char* from, const char* current, const char* next) {
    if (!current || station(current) || bayGate(current) || dockingReverse(from,current)) return false;
    const auto incoming = edgeHeading(from,current,false,true), outgoing = edgeHeading(current,next);
    return incoming != RobotHeading::Unknown && incoming == outgoing;
}
inline bool physicalBranch(const char* current, RobotHeading heading) {
    for (const auto& edge : RouteMap::EDGES) {
        if (std::strcmp(edge.from,current) == 0 && edgeHeading(edge.from,edge.to) == heading) return true;
        if (std::strcmp(edge.to,current) == 0 && edgeHeading(edge.from,edge.to,true,true) == heading) return true;
    }
    return false;
}
inline bool reverseAllowed(const char* current, const CoreCommand& command, const RoutePlan& route,
                           bool calibrated = Config::REVERSE_STEERING_CONFIRMED,
                           bool bayTrial = Config::ENABLE_CORE_MOTION) {
    if (command.pathLength != route.count) return false;
    for (uint8_t i=0;i<route.count;++i)
        if ((route.departure[i] == RouteTurn::Reverse || route.reverseAfterTurn[i]) && !calibrated &&
            !(bayTrial && (dockingReverse(i ? command.path[i-1] : current,command.path[i]) ||
                          reverseHome(i ? command.path[i-1] : current,command.path[i])))) return false;
    return true;
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
    return turnFromHeading(edgeHeading(previous,current,false,true),edgeHeading(current,next));
}

inline bool plan(const char* previous, const char* current, const CoreCommand& command,
                 RoutePlan& output, bool previousWasReverse = false,
                 RobotHeading confirmedHeading = RobotHeading::Unknown) {
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
        const RobotHeading docking = bayHeading(from);
        const bool explicitHeading = docking != RobotHeading::Unknown ||
                                     (i == 0 && confirmedHeading != RobotHeading::Unknown);
        if (explicitHeading) decision = turnFromHeading(
            docking != RobotHeading::Unknown ? docking : confirmedHeading, edgeHeading(from,to));
        if (previousWasReverse && !explicitHeading) {
            if (decision == RouteTurn::Left) decision = RouteTurn::Right;
            else if (decision == RouteTurn::Right) decision = RouteTurn::Left;
            else if (decision == RouteTurn::Reverse) decision = RouteTurn::Straight;
            else if (decision == RouteTurn::Straight) decision = RouteTurn::Reverse; // continue backing on the same line
        }
        output.reverseAfterTurn[i] = reverseHome(from,to);
        if (output.reverseAfterTurn[i]) {
            // Face away from S1 on the user-confirmed GS1 line extension.
            // Turn is physical rotation; translation is independently backward.
            const auto front = explicitHeading ? (docking != RobotHeading::Unknown ? docking : confirmedHeading) :
                edgeHeading(incoming,from,previousWasReverse,true);
            decision = turnFromHeading(front,edgeHeading(from,to,true));
        }
        if (decision == RouteTurn::Reverse && !dockingReverse(from,to)) {
            decision = RouteTurn::TurnAround;
        }
        if (decision == RouteTurn::TurnAround) {
            const auto outgoing = edgeHeading(from,to,output.reverseAfterTurn[i]);
            const auto halfway = static_cast<RobotHeading>((static_cast<int>(outgoing)+2)%4+1);
            output.turnCrossings[i] = 1 + (physicalBranch(from,halfway) ? 1 : 0);
        }
        output.departure[i] = decision;
        previousWasReverse = decision == RouteTurn::Reverse || output.reverseAfterTurn[i];
        // A claimed previous RFID that is inconsistent with the one-way graph is unsafe.
        if (incoming && incoming[0] && output.departure[i] == RouteTurn::Unknown) return false;
        from = to;
    }
    output.count = command.pathLength;
    return true;
}
}
