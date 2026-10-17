#include "core/RuntimeStore.hpp"
#include "core/FLeetmanager.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(value) do {if (!(value)) throw std::runtime_error(#value);} while (false)

int main(int argc,char** argv) {
    try {
        MapGraph map;map.loadFromFile("data/map_layout.json");const auto current = map.toJson();
        json old;
        if (argc > 1) {std::ifstream in(argv[1]);in >> old;}
        else {
            MapGraph legacy;legacy.loadFromFile("data/map_layout.json");
            old = {{"version",1},{"map",legacy.toJson()}};
            auto& graph = old["map"];graph["map_name"] = "Kho AGV v3 (cm)";graph["revision"] = 2;
            struct Corner {const char* id;const char* from;const char* to;int x;int y;int first;int second;};
            for (const auto& c : {Corner{"C1","G1","M1",0,0,70,50}, {"C2","M2","G6",200,0,50,70},
                {"C3","M3","G1",0,140,50,70}, {"C4","G6","M4",200,140,70,50}}) {
                graph["nodes"].push_back({{"id",c.id},{"x",c.x},{"y",c.y},{"type","waypoint"}});
                auto& edges = graph["edges"];
                edges.erase(std::remove_if(edges.begin(),edges.end(),[&](const json& e){return e["from"] == c.from && e["to"] == c.to;}),edges.end());
                edges.push_back({{"from",c.from},{"to",c.id},{"weight",c.first},{"type","main"},{"two_way",false},{"path_class",c.first == 70 ? "outer" : "boulevard"}});
                edges.push_back({{"from",c.id},{"to",c.to},{"weight",c.second},{"type","main"},{"two_way",false},{"path_class",c.second == 70 ? "outer" : "boulevard"}});
            }
            MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setNetworkAvailable(false);fleet.updateTelemetry("01","G1",80,false);
            fleet.addTask("M1",1,"pending");
            old["fleet"] = fleet.durableSnapshot();old["fleet"]["agvs"][0]["node"] = "C1";
            old["fleet"]["agvs"][0]["path"] = json::array({"C2","G6"});
            old["fleet"]["tasks"].push_back({{"task_id","old-corner"},{"target","C2"},{"status","completed"}});
            arbiter.setGrantIdAllocator([]{return 123L;});arbiter.requestZone("M1","01");arbiter.requestZone("M1","02");
            old["mutex"] = arbiter.toJson();
        }
        const auto original = old;CHECK(!RuntimeStore::mapsCompatible(current,old.at("map")));
        CHECK(RuntimeStore::migrateRoundedMap(current,old));CHECK(RuntimeStore::mapsCompatible(current,old.at("map")));
        CHECK(old.at("fleet").at("pending_tasks").empty());
        CHECK(old.at("fleet").at("tasks").size() == original.at("fleet").at("tasks").size());
        for (const auto& zone : original.at("mutex").items()) {
            CHECK(old.at("mutex").at(zone.key()).at("owner") == zone.value().at("owner"));
            if (zone.value().contains("grant_id")) CHECK(old.at("mutex").at(zone.key()).at("grant_id") == zone.value().at("grant_id"));
            CHECK(old.at("mutex").at(zone.key()).at("waiting").empty());
        }
        CHECK(old.at("fleet").at("agvs")[0].at("node") == "");
        for (const auto& robot : old.at("fleet").at("agvs")) {
            CHECK(robot.at("position_valid") == false);
            CHECK(robot.at("uncertain_path").size() >= 2);
            for (const auto& n : robot.at("uncertain_path")) CHECK(map.nodeExists(n.get<std::string>()));
        }
        MutexArbiter restoredArbiter(map.mutexZones());restoredArbiter.restore(old.at("mutex"));
        FleetManager restored(map,restoredArbiter);restored.restore(old.at("fleet"));
        CHECK(restored.toJson().at("agvs")[0].at("position_valid") == false);
        CHECK(restored.toJson().at("pending_tasks").empty());
        const auto restoredData = restored.durableSnapshot();
        FleetManager again(map,restoredArbiter);again.restore(restoredData); // Retired history survives the next restart.
        auto wrong = original;wrong["map"]["edges"][0]["weight"] = 999;
        const auto before = wrong;CHECK(!RuntimeStore::migrateRoundedMap(current,wrong));CHECK(wrong == before);
        wrong = original;wrong["map"]["obstacles"] = json::array({"C1"});
        CHECK(!RuntimeStore::migrateRoundedMap(current,wrong));
        wrong = old;CHECK(!RuntimeStore::migrateRoundedMap(current,wrong));
        const auto folder = std::filesystem::path(".qa")/("runtime-migration-"+std::to_string(Clock::now().time_since_epoch().count()));
        RuntimeStore store(folder/"runtime.json");store.save(original);
        const auto backup = store.backupBeforeRoundedMigration();store.save(old);
        std::ifstream in(backup);json preserved;in >> preserved;CHECK(preserved == original);
        CHECK(store.load().value() == old);
        std::cout << "PASS rounded-map checkpoint migration: history, mutex owners/tokens, uncertain paths, restart, backup, reject unrelated changes\n";
        return 0;
    } catch (const std::exception& e) {std::cerr << e.what() << "\n";return 1;}
}
