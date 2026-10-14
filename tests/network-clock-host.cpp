#include "network/NetworkHealth.h"
#include <cassert>
#include <iostream>
int main() {
    uint32_t clock=1000,service=999,heartbeat=999;
    const uint32_t oldNow=clock;
    const auto status=[&] {
        // WiFi.status() / another core runs across a millis boundary while sampling health.
        clock=1001;service=1001;heartbeat=1001;
        return NetworkHealth::Snapshot{true,true,true,false,service,heartbeat};
    };
    const auto concurrent=status();
    assert(static_cast<uint32_t>(oldNow-concurrent.servicedAt)>=Config::NETWORK_WORKER_STALE_MS);
    assert(NetworkHealth::connected(status,[&]{return clock;}));
    const auto check=[](NetworkHealth::Snapshot s,uint32_t now){return NetworkHealth::connected([&]{return s;},[&]{return now;});};
    assert(check({true,true,true,false,1000,1000},1000));
    assert(check({true,true,true,false,1000,1000},2999));
    assert(!check({true,true,true,false,1000,1000},3000)); // actual worker timeout still stops
    assert(!check({true,true,true,false,4000,1000},4000)); // actual heartbeat timeout still stops
    assert(!check({false,true,true,false,1000,1000},1000));
    assert(!check({true,false,true,false,1000,1000},1000));
    assert(!check({true,true,false,false,1000,1000},1000));
    assert(!check({true,true,true,true,1000,1000},1000));
    assert(check({true,true,true,false,UINT32_MAX-10,UINT32_MAX-10},10));
    std::cout << "PASS network health clock: reproduce old false-offline race; snapshot-before-clock, actual timeouts/disconnect/restart and millis wrap\n";
}
