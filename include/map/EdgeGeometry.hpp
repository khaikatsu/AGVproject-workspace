#pragma once
#include "utils/json.hpp"
#include <algorithm>
#include <cmath>

// Geometry is a drawing/heading aid. Segment lengths, not pixel distances,
// determine cost and encoder progress. The via point is not an RFID node.
namespace EdgeGeometry {
using json = nlohmann::json;
inline const json* node(const json& map, const std::string& id) {
    for (const auto& n : map.at("nodes")) if (n.at("id") == id) return &n;
    return nullptr;
}
inline int direction(const json& a, const json& b) {
    const double dx=b.at("x").get<double>()-a.at("x").get<double>();
    const double dy=b.at("y").get<double>()-a.at("y").get<double>();
    if (dx == 0 && dy != 0) return dy < 0 ? 0 : 2;
    if (dy == 0 && dx != 0) return dx < 0 ? 3 : 1;
    return -1;
}
inline int heading(const json& map, const std::string& from, const std::string& to, bool arrival=false) {
    const auto* a=node(map,from); const auto* b=node(map,to);
    if (!a || !b) return -1;
    for (const auto& e : map.at("edges")) {
        const bool forward=e.at("from")==from && e.at("to")==to;
        const bool backward=e.value("two_way",false) && e.at("from")==to && e.at("to")==from;
        if ((forward || backward) && e.contains("geometry")) {
            const auto& via=e.at("geometry").at("via");
            return arrival ? direction(via,*b) : direction(*a,via);
        }
    }
    return direction(*a,*b);
}
inline json point(const json& map, const json& edge, const std::string& from, double distance) {
    const bool forward=edge.at("from")==from;
    const auto* a=node(map,edge.at(forward ? "from" : "to").get<std::string>());
    const auto* b=node(map,edge.at(forward ? "to" : "from").get<std::string>());
    if (!a || !b) return nullptr;
    double length=edge.at("weight").get<double>();
    const json* start=a; const json* end=b;
    distance=std::clamp(distance,0.0,length);
    if (edge.contains("geometry")) {
        const auto& g=edge.at("geometry"); const auto& via=g.at("via");
        const double first=g.at("segment_lengths_cm").at(forward ? 0 : 1).get<double>();
        if (distance <= first) {end=&via;length=first;}
        else {start=&via;distance-=first;length-=first;}
    }
    const double ratio=length > 0 ? distance/length : 0;
    return {{"x",start->at("x").get<double>()+(end->at("x").get<double>()-start->at("x").get<double>())*ratio},
            {"y",start->at("y").get<double>()+(end->at("y").get<double>()-start->at("y").get<double>())*ratio}};
}
}
