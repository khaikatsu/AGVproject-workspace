#pragma once
#include "Types.h"
#include <cstring>

// Coarse graph occupancy only: a node or both endpoints of the last segment.
// Never turn tracking=false after a fault into proof of being at the last RFID.
class StoppedPosition {
public:
    void update(const OdometrySnapshot& before, const char* node, uint32_t session,
                bool localized, bool fault, bool stopped, bool pivot, uint32_t rfidAgeMs) {
        const uint32_t now = before.uptimeMs;
        if (!localized || session_ != session || std::strcmp(node_,node)) {
            valid_ = waiting_ = false;
            session_ = session;
            std::strncpy(node_,node,sizeof(node_)-1);
        }
        if (!localized) return;
        if (!fault) {
            waiting_ = false;
            lastPivot_=pivot;
            valid_ = before.tracking && before.localSession==session && !pivot && !std::strcmp(before.fromNode,node);
            if (valid_) {
                std::strncpy(from_,before.fromNode,sizeof(from_)-1);
                std::strncpy(to_,before.toNode,sizeof(to_)-1);
                route_ = before.routeCmdId; index_ = before.pathIndex;
                distance_ = before.distanceCm;
            }
            return;
        }
        if (!stopped || pivot) { valid_ = waiting_ = false; return; }
        if (!waiting_) {
            // A fault can happen before the first normal encoder context is sent.
            if (before.tracking && before.localSession==session && !lastPivot_ && !std::strcmp(before.fromNode,node)) {
                valid_ = true;
                std::strncpy(from_,before.fromNode,sizeof(from_)-1);
                std::strncpy(to_,before.toNode,sizeof(to_)-1);
                route_ = before.routeCmdId; index_ = before.pathIndex;
                distance_ = before.distanceCm;
            }
            waiting_ = true; stillAt_ = now;
            left_ = before.leftTicks; right_ = before.rightTicks;
        }
        if (left_ != before.leftTicks || right_ != before.rightTicks) {
            if (to_[0]) distance_ += (static_cast<double>(before.leftTicks-left_)+
                static_cast<double>(before.rightTicks-right_))*50.0/Config::ENCODER_PULSES_PER_100CM;
            else valid_ = false;
            left_ = before.leftTicks; right_ = before.rightTicks; stillAt_ = now;
        }
        // A newly read tag during the stopped fault is an explicit node proof.
        if (rfidAgeMs <= 250 && node[0]) {
            valid_ = true; std::strncpy(from_,node,sizeof(from_)-1);
            to_[0] = 0; route_ = 0; index_ = 0; distance_ = 0;
        }
    }
    void attach(OdometrySnapshot& sample) const {
        if (!valid_ || !waiting_ || sample.localSession != session_ ||
            sample.leftTicks != left_ || sample.rightTicks != right_ ||
            static_cast<uint32_t>(sample.uptimeMs-stillAt_) < 600) return;
        sample.stopPositionValid = true;
        sample.stationaryMs = sample.uptimeMs-stillAt_;
        std::strncpy(sample.stopFrom,from_,sizeof(sample.stopFrom)-1);
        std::strncpy(sample.stopTo,to_,sizeof(sample.stopTo)-1);
        sample.stopRoute = route_; sample.stopIndex = index_;
        sample.stopDistanceCm = distance_;
    }
private:
    bool valid_ = false, waiting_ = false, lastPivot_ = false;
    uint32_t session_ = 0, stillAt_ = 0, left_ = 0, right_ = 0;
    char node_[NODE_TEXT_SIZE]{}, from_[NODE_TEXT_SIZE]{}, to_[NODE_TEXT_SIZE]{};
    int32_t route_ = 0;
    float distance_ = 0;
    uint8_t index_ = 0;
};
