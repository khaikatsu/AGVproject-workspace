#pragma once
#include "utils/json.hpp"
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

// The flashed firmware's RFID route table uses these physical roles. Keep the
// contract in one place; renaming a role requires rebuilding firmware as well.
namespace PhysicalLayout {
inline std::string home(const std::string& id) {return id=="01" ? "S1" : id=="02" ? "S2" : "";}
inline std::unordered_map<std::string,std::string> homes() {return {{"01",home("01")},{"02",home("02")}};}
inline bool isHome(const std::string& node) {return node==home("01") || node==home("02");}
inline std::string outwardHeading(const std::string& node) {return node==home("01") ? "S" : node==home("02") ? "N" : "";}
inline std::string gate(const std::string& node) {
    if(node==home("01")) return "GS1";
    if(node==home("02")) return "GS2";
    if(node.size()==2 && node[0]=='K' && node[1]>='1' && node[1]<='6') return "G"+node.substr(1);
    return "";
}
inline bool warehouse(const std::string& node) {return !isHome(node) && !gate(node).empty();}
inline void validate(const nlohmann::json& map) {
    const auto& nodes=map.at("nodes");const auto& edges=map.at("edges");
    for(const auto* role:{"S1","S2","K1","K2","K3","K4","K5","K6"}) {
        const std::string id=role, exit=gate(id);
        auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.at("id")==id;});
        if(node==nodes.end() || node->value("type",std::string{})!=(isHome(id)?"charging":"station"))
            throw std::invalid_argument("Layout firmware thieu/sai vai tro "+id);
        const bool outbound=std::any_of(edges.begin(),edges.end(),[&](const auto& e){
            return (e.at("from")==id && e.at("to")==exit) ||
                (e.value("two_way",false) && e.at("to")==id && e.at("from")==exit);
        });
        const bool inbound=std::any_of(edges.begin(),edges.end(),[&](const auto& e){
            return (e.at("from")==exit && e.at("to")==id) ||
                (e.value("two_way",false) && e.at("to")==exit && e.at("from")==id);
        });
        if(!outbound || !inbound) throw std::invalid_argument("Layout firmware thieu cap canh "+id+" <-> "+exit);
    }
}
}
