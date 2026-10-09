#include "map/MapGraph.hpp"
#include <thread>
#include <chrono>

void simulateAgv(const std::string& agvName, const std::string& zoneId, MutexManager& mgr) {
    std::cout << "[" << agvName << "] muon di qua " << zoneId << "...\n";
    mgr.enter(zoneId); // block cho den khi chiem duoc
    std::cout << "[" << agvName << "] DA CHIEM " << zoneId << ", dang di qua...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[" << agvName << "] roi khoi " << zoneId << "\n";
    mgr.leave(zoneId);
}

int main() {
    MapGraph map;
    try {
        map.loadFromFile("data/map_layout.json");
    } catch (const std::exception& ex) {
        std::cerr << "Loi nap map: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "== Ban do: " << map.mapName() << " ==\n";
    std::cout << "So node: " << map.allNodes().size() << "\n";
    std::cout << "So edge: " << map.allEdges().size() << "\n";

    std::cout << "\nMutex zones:\n";
    for (const auto& id : map.mutexZones()) {
        const auto& n = map.node(id);
        std::cout << "  - " << id << " (" << n.x << ", " << n.y << ")\n";
    }

    std::cout << "\nTram sac:\n";
    for (const auto& id : map.chargingStations()) {
        std::cout << "  - " << id << "\n";
    }

    std::cout << "\nDuong ke tu M1:\n";
    for (const auto& e : map.neighbors("M1")) {
        std::cout << "  M1 -> " << e.to << " [" << e.type
                   << (e.path_class.empty() ? "" : "/" + e.path_class) << "]\n";
    }

    std::cout << "\n== Mo phong 2 AGV tranh chap Mutex Zone M1 ==\n";
    MutexManager mgr(map.mutexZones());
    std::thread t1(simulateAgv, "AGV_01", "M1", std::ref(mgr));
    std::thread t2(simulateAgv, "AGV_02", "M1", std::ref(mgr));
    t1.join();
    t2.join();

    return 0;
} 
