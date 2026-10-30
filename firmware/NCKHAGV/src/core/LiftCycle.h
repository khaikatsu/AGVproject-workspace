#pragma once
#include "Types.h"
#include <cstring>

// Timed target operation; elapsed time is not a position/load measurement.
class LiftCycle {
public:
    struct Record {uint32_t version=2;int32_t id=0;uint64_t owner=0;bool up=false;uint8_t phase=0;char node[NODE_TEXT_SIZE]{};};
    void setOwner(uint64_t owner){owner_=owner;}
    uint64_t owner() const {return owner_;}
    Record record() const {
        Record r;r.id=cmdId_;r.owner=owner_;r.up=up_;std::memcpy(r.node,node_,sizeof(node_));
        r.phase=busy_||resumePending_?1:exitPending_?2:known_?3:std::strcmp(state_,"IDLE")==0?5:4;return r;
    }
    bool restore(const Record& r) {
        if(r.version!=2||r.id<=0||!r.owner||r.node[0]!='K'||r.node[1]<'1'||r.node[1]>'6'||r.node[2]||r.phase<1||r.phase>5)return false;
        owner_=r.owner;
        cmdId_=r.id;up_=r.up;std::memcpy(node_,r.node,sizeof(node_));
        resumePending_=r.phase==1;exitPending_=r.phase==2;known_=r.phase==2||r.phase==3;
        loaded_=exitPending_||up_;state_=r.phase==5?"IDLE":resumePending_?"WAITING_LIFT_RECOVERY":exitPending_?"LOWERED":known_?(up_?"DONE_UP":"DONE_DOWN"):"INTERRUPTED";
        return true;
    }
    bool resumePending() const {return resumePending_;}
    void abandonAtMaintenance() {busy_=resumePending_=exitPending_=exitWaiting_=known_=false;state_="IDLE";}
    void pauseForRecovery() {if(busy_) {busy_=false;resumePending_=true;known_=false;state_="WAITING_LIFT_RECOVERY";}}
    void resume(uint32_t now) {resumePending_=false;busy_=true;started_=now;state_=up_?"LIFTING":"LOWERING";}
    bool start(const CoreCommand& command,uint32_t now) {
        if (busy_ || resumePending_ || exitPending_ || command.cmdId <= 0 ||
            (command.type != CoreCommandType::LiftUp && command.type != CoreCommandType::LiftDown)) return false;
        up_=command.type==CoreCommandType::LiftUp;cmdId_=command.cmdId;started_=now;
        std::memcpy(node_,command.node,sizeof(node_));
        busy_=true;state_=up_ ? "LIFTING" : "LOWERING";return true;
    }
    bool update(uint32_t now,bool safe,bool /*sensorStable*/,bool /*sensorPresent*/) {
        if (!busy_) return false;
        if (!safe) { interrupt();return true; }
        if (static_cast<uint32_t>(now-started_) < 5000) return false;
        // Cargo is monitoring only; the matching timed cycle completes after 5s.
        busy_=false;known_=true;loaded_=true;exitPending_=!up_;
        state_=up_ ? "DONE_UP" : "LOWERED";return true;
    }
    bool exitRouteAllowed(const char* current,const CoreCommand& command) const {
        return exitPending_ && std::strcmp(current,node_)==0 && command.pathLength==1 &&
            node_[0]=='K' && node_[1]>='1' && node_[1]<='6' && node_[2]=='\0' &&
            command.target[0]=='G' && command.target[1]==node_[1] && command.target[2]=='\0' &&
            std::strcmp(command.path[0],command.target)==0;
    }
    // Lowering finishes under the rack. Complete delivery after STOP at its exit gate.
    bool observeExit(uint32_t /*now*/,const char* current,bool safe,bool /*stable*/,bool /*present*/) {
        const bool atGate=current[0]=='G' && current[1]==node_[1] && current[2]=='\0';
        if(!exitPending_ || !atGate) {exitWaiting_=false;return false;}
        if(!safe)return false;
        exitPending_=false;exitWaiting_=false;known_=true;loaded_=false;state_="DONE_DOWN";return true;
    }
    bool exitPending() const {return exitPending_;}
    void interrupt() { if(busy_||resumePending_) {busy_=false;resumePending_=false;known_=false;state_="INTERRUPTED";} }
    bool busy() const {return busy_;}
    bool up() const {return up_;}
    bool cargoKnown() const {return known_;}
    bool cargo() const {return loaded_;}
    int32_t cmdId() const {return cmdId_;}
    const char* node() const {return node_;}
    const char* state() const {return state_;}
    uint32_t elapsed(uint32_t now) const {return busy_ ? static_cast<uint32_t>(now-started_) : known_ ? 5000 : 0;}
private:
    bool busy_=false,up_=false,known_=false,loaded_=false,resumePending_=false;
    bool exitPending_=false,exitWaiting_=false;
    int32_t cmdId_=0;uint32_t started_=0;
    uint64_t owner_=0;
    char node_[NODE_TEXT_SIZE]{};
    const char* state_="IDLE";
};
