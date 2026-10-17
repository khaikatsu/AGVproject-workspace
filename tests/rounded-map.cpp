#include "map/MapGraph.hpp"
#include "core/NodeManeuver.hpp"
#include "core/CoreNavigator.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error("Failed: " #c); } while(false)

int main() {
    try {
        MapGraph map;map.loadFromFile("data/map_layout.json");
        const auto layout=map.toJson();
        CHECK(layout["nodes"].size()==22);CHECK(Config::RFID_NODES.size()==22);
        size_t turns=0,bends=0;
        for (const auto& n : layout["nodes"]) CHECK(RoutePlanner::node(n.at("id").get<std::string>().c_str()));
        for (const auto& e : layout["edges"]) {
            const auto from=e.at("from").get<std::string>(),to=e.at("to").get<std::string>();
            CHECK(RoutePlanner::directedEdge(from.c_str(),to.c_str()));
            CHECK(!RoutePlanner::directedEdge(to.c_str(),from.c_str()) || e.value("two_way",false));
            CHECK(static_cast<int>(RoutePlanner::edgeHeading(from.c_str(),to.c_str()))==EdgeGeometry::heading(layout,from,to)+1);
            CHECK(static_cast<int>(RoutePlanner::edgeHeading(from.c_str(),to.c_str(),false,true))==EdgeGeometry::heading(layout,from,to,true)+1);
            for (const auto& next : layout["edges"]) {
                if (next.at("from")!=to) continue;
                const auto target=next.at("to").get<std::string>();
                const int incoming=EdgeGeometry::heading(layout,from,to,true);
                const auto front=std::string(incoming==0?"N":incoming==1?"E":incoming==2?"S":"W");
                const auto order=NodeManeuver::plan(layout,to,target,front);
                CHECK(!order.is_null());
                const auto turn=RoutePlanner::turn(from.c_str(),to.c_str(),target.c_str());
                const auto name=std::string(RoutePlanner::turnName(turn));
                // Backing out of a warehouse is decided by its fixed bay heading.
                if (to[0]!='K' && turn!=RouteTurn::Reverse) CHECK(order.at("maneuver")==name);
                CHECK(NodeManeuver::straightThrough(layout,from,to,target)==RoutePlanner::straightThrough(from.c_str(),to.c_str(),target.c_str()));
                ++turns;
            }
            if (!e.contains("geometry")) continue;
            ++bends;CHECK(e.at("weight")==135);
            const auto first=e.at("geometry").at("segment_lengths_cm")[0].get<double>();
            const auto via=e.at("geometry").at("via");
            CHECK(EdgeGeometry::point(layout,e,from,first)==via);
            CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=123;c.pathLength=1;
            AGVMath::copyText(c.path[0],sizeof(c.path[0]),to.c_str());
            AGVMath::copyText(c.target,sizeof(c.target),to.c_str());
            RoutePlan plan;
            CHECK(RoutePlanner::plan("",from.c_str(),c,plan,false,RoutePlanner::edgeHeading(from.c_str(),to.c_str())));
            CHECK(plan.departure[0]==RouteTurn::Straight);
            CoreNavigator nav;CHECK(nav.start(from.c_str(),c,plan,0));
            if (from[0]=='M') CHECK(nav.grant(from.c_str(),499));
            if (to[0]=='M') CHECK(nav.grant(to.c_str(),500));
            ControlInput in;in.wifi=in.mqtt=in.distanceValid=in.motionConfigured=in.lineConfigured=true;
            in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS+1;
            CHECK(nav.update(in)==MotorAction::Forward);
            in.now+=100;CHECK(nav.update(in)==MotorAction::Forward);CHECK(nav.active());
            nav.reachedNode(to.c_str(),++in.now,&in);CHECK(!nav.active());
            CHECK(nav.update(in)==MotorAction::Stop);
        }
        CHECK(bends==4);
        const auto before=map.toJson();
        for (int bad=0;bad<4;++bad) {
            auto invalid=layout;
            for (auto& e : invalid["edges"]) if (e.contains("geometry")) {
                if (bad==0) e["geometry"]["segment_lengths_cm"][0]=0;
                if (bad==1) e["geometry"]["segment_lengths_cm"][1]=99;
                if (bad==2) e["geometry"]["via"]["x"]=12;
                if (bad==3) e["geometry"]["via"]["y"]="bad";
                break;
            }
            bool rejected=false;try {map.loadFromJson(invalid);} catch (...) {rejected=true;}
            CHECK(rejected);CHECK(map.toJson()==before);
        }
        map.addEdge("G1","M1",135,"main","outer",false);
        CHECK(map.toJson()["edges"].back().contains("geometry"));
        std::cout << "PASS map v4 profile " << Config::AGV_ID << ": 22 nodes, 4 bends, " << turns
                  << " Core/MCU route triples, continuous curve following, validation/roundtrip\n";
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
