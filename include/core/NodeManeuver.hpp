#pragma once
#include "utils/json.hpp"
#include "map/EdgeGeometry.hpp"
#include "core/PhysicalLayout.hpp"
#include <string>

// Map y points down. Heading is the vehicle front, including during reverse.
namespace NodeManeuver {
using json = nlohmann::json;
inline int heading(const std::string& value) {
    return value == "N" ? 0 : value == "E" ? 1 : value == "S" ? 2 : value == "W" ? 3 : -1;
}
inline int direction(const json& map, const std::string& from, const std::string& to) {
    return EdgeGeometry::heading(map,from,to);
}
inline bool branch(const json& map, const std::string& node, int dir, bool directed = false) {
    for (const auto& edge : map.at("edges")) {
        if (edge.at("from") == node && direction(map,node,edge.at("to")) == dir) return true;
        if (edge.at("to") == node && (!directed || edge.value("two_way",false))) {
            const int incoming=EdgeGeometry::heading(map,edge.at("from"),node,true);
            if (incoming>=0 && (incoming+2)%4==dir) return true;
        }
    }
    return false;
}
// Authorization belongs to a route triple, not to a node's global degree.
inline bool straightThrough(const json& map, const std::string& from,
                            const std::string& node, const std::string& next) {
    if (node.size() == 2 && node[0] == 'G' && node[1] >= '1' && node[1] <= '6') return false;
    bool allowedType = false, backingOut = false;
    for (const auto& n : map.at("nodes")) {
        if (n.at("id") == from && n.value("type",std::string{}) == "station") backingOut = true;
        if (n.at("id") == node) {
            const auto type = n.value("type",std::string{});
            allowedType = type == "waypoint" || type == "mutex";
        }
    }
    const int incoming = EdgeGeometry::heading(map,from,node,true), outgoing = direction(map,node,next);
    auto connected = [&](const std::string& a, const std::string& b) {
        for (const auto& e : map.at("edges"))
            if ((e.at("from") == a && e.at("to") == b) ||
                (e.value("two_way",false) && e.at("from") == b && e.at("to") == a)) return true;
        return false;
    };
    return allowedType && !backingOut && incoming >= 0 && incoming == outgoing &&
           connected(from,node) && connected(node,next);
}
inline json plan(const json& map, const std::string& node, const std::string& next,
                 const std::string& front, bool stationTurn = false, bool reverseHomeSupported = false) {
    const int incoming = heading(front);
    if (incoming < 0) return nullptr;
    if (stationTurn) {
        const auto gate = PhysicalLayout::isHome(node) ? PhysicalLayout::gate(node) : "";
        if (!gate.empty() && next == node && direction(map,node,gate) == (incoming+2)%4)
            return {{"maneuver","STATION_TURN_RIGHT"},{"turn_crossings",1}};
        return nullptr;
    }
    const int outgoing = direction(map,node,next);
    bool connected = false;
    for (const auto& edge : map.at("edges"))
        if ((edge.at("from") == node && edge.at("to") == next) ||
            (edge.value("two_way",false) && edge.at("to") == node && edge.at("from") == next)) connected = true;
    if (outgoing < 0 || !connected) return nullptr;
    const int delta = (outgoing-incoming+4)%4;
    if (delta == 0) return {{"maneuver","STRAIGHT"},{"turn_crossings",0}};
    if (delta == 1) return {{"maneuver","RIGHT"},{"turn_crossings",1}};
    if (delta == 3) return {{"maneuver","LEFT"},{"turn_crossings",1}};
    const bool warehouseExit = PhysicalLayout::warehouse(node) && next==PhysicalLayout::gate(node);
    if (warehouseExit) return {{"maneuver","REVERSE"},{"turn_crossings",0}};
    return {{"maneuver","TURN_AROUND_RIGHT"},{"turn_crossings",1+(branch(map,node,(incoming+1)%4) ? 1 : 0)}};
}
}
