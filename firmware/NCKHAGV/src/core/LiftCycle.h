#pragma once
#include "Types.h"
#include <cstring>

// Timed PWM operation, not a position/load measurement. No replay on restart.
class LiftCycle {
public:
    bool start(const CoreCommand& command,uint32_t now) {
        if (busy_ || exitPending_ || command.cmdId <= 0 ||
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
        if(up_ && (!sensorStable || !sensorPresent)) {
            if(static_cast<uint32_t>(now-started_)<7000)return false;
            interrupt();return true;
        }
        busy_=false;known_=true;loaded_=true;exitPending_=!up_;
        state_=up_ ? "DONE_UP" : "LOWERED";return true;
    }
    bool exitRouteAllowed(const char* current,const CoreCommand& command) const {
        return exitPending_ && std::strcmp(current,node_)==0 && command.pathLength==1 &&
            node_[0]=='K' && node_[1]>='1' && node_[1]<='6' && node_[2]=='\0' &&
            command.target[0]=='G' && command.target[1]==node_[1] && command.target[2]=='\0' &&
            std::strcmp(command.path[0],command.target)==0;
    }
    // Lowering finishes under the rack. Require STOP at the exit gate plus an empty sensor.
    bool observeExit(uint32_t now,const char* current,bool safe,bool stable,bool present) {
        const bool atGate=current[0]=='G' && current[1]==node_[1] && current[2]=='\0';
        if(!exitPending_ || !atGate) {exitWaiting_=false;return false;}
        if(!safe)return false;
        if(!exitWaiting_) {exitWaiting_=true;exitStarted_=now;}
        if(stable && !present) {
            exitPending_=false;exitWaiting_=false;known_=true;loaded_=false;state_="DONE_DOWN";return true;
        }
        if(static_cast<uint32_t>(now-exitStarted_)<2000)return false;
        known_=false;state_="INTERRUPTED";return true;
    }
    bool exitPending() const {return exitPending_;}
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
    bool exitPending_=false,exitWaiting_=false;
    uint32_t exitStarted_=0;
    int32_t cmdId_=0;uint32_t started_=0;
    char node_[NODE_TEXT_SIZE]{};
    const char* state_="IDLE";
};
