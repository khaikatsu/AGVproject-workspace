#pragma once
#include "Types.h"
#include <cstring>

// Timed PWM operation, not a position/load measurement. No replay on restart.
class LiftCycle {
public:
    bool start(const CoreCommand& command,uint32_t now) {
        if (busy_ || command.cmdId <= 0 ||
            (command.type != CoreCommandType::LiftUp && command.type != CoreCommandType::LiftDown)) return false;
        up_=command.type==CoreCommandType::LiftUp;cmdId_=command.cmdId;started_=now;
        std::memcpy(node_,command.node,sizeof(node_));
        busy_=true;state_=up_ ? "LIFTING" : "LOWERING";return true;
    }
    bool update(uint32_t now,bool safe,bool sensorStable,bool sensorPresent) {
        if (!busy_) return false;
        if (!safe) { interrupt();return true; }
        if (static_cast<uint32_t>(now-started_) < 5000) return false;
        // Bound the sensor wait; never repeat the physical servo action.
        if(!sensorStable || sensorPresent!=up_) {
            if(static_cast<uint32_t>(now-started_)<7000)return false;
            interrupt();return true;
        }
        busy_=false;known_=true;loaded_=up_;state_=up_ ? "DONE_UP" : "DONE_DOWN";return true;
    }
    void interrupt() { if(busy_) {busy_=false;known_=false;state_="INTERRUPTED";} }
    bool busy() const {return busy_;}
    bool up() const {return up_;}
    bool cargoKnown() const {return known_;}
    bool cargo() const {return loaded_;}
    int32_t cmdId() const {return cmdId_;}
    const char* node() const {return node_;}
    const char* state() const {return state_;}
    uint32_t elapsed(uint32_t now) const {return busy_ ? static_cast<uint32_t>(now-started_) : known_ ? 5000 : 0;}
private:
    bool busy_=false,up_=false,known_=false,loaded_=false;
    int32_t cmdId_=0;uint32_t started_=0;
    char node_[NODE_TEXT_SIZE]{};
    const char* state_="IDLE";
};
