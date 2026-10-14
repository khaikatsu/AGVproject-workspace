#pragma once
#include "utils/json.hpp"
#include <cmath>
#include <cstdint>
#include <climits>
#include <chrono>
#include <string>
#include <unordered_set>

struct EncoderSample {
    uint32_t boot = 0, connection = 0, seq = 0, uptime = 0, leftTicks = 0, rightTicks = 0;
    long route = 0;
    int index = 0;
    std::string from, to, coreSession;
    bool tracking = false, translating = false, reverse = false;
    double distance = 0, speed = 0, pulsesPer100Cm = 0;
    static EncoderSample parse(const nlohmann::json& j) {
        using json = nlohmann::json;
        const auto integer = [&](const char* key, uint64_t maximum, uint64_t minimum = 0) -> uint32_t {
            const auto& v = j.at(key);
            if (!v.is_number_integer() || (v.is_number_integer() && !v.is_number_unsigned() && v.get<int64_t>() < 0))
                throw std::invalid_argument("Odometry integer sai");
            const auto n = v.get<uint64_t>();
            if (n < minimum || n > maximum) throw std::invalid_argument("Odometry integer ngoai gioi han");
            return static_cast<uint32_t>(n);
        };
        const auto number = [&](const char* key, double maximum, double minimum = 0) {
            const auto& v = j.at(key);
            if (!v.is_number()) throw std::invalid_argument("Odometry number sai");
            const double n = v.get<double>();
            if (!std::isfinite(n) || n < minimum || n > maximum) throw std::invalid_argument("Odometry number ngoai gioi han");
            return n;
        };
        const auto boolean = [&](const char* key) {
            if (!j.at(key).is_boolean()) throw std::invalid_argument("Odometry boolean sai");
            return j.at(key).get<bool>();
        };
        if (!j.is_object() || integer("schema_version", 1, 1) != 1 || j.at("robot_id") != "01")
            throw std::invalid_argument("Odometry chi ho tro xe 01/schema 1");
        EncoderSample s;
        s.boot = integer("boot_id", UINT32_MAX, 1); s.connection = integer("connection_session", UINT32_MAX, 1);
        s.seq = integer("seq", UINT32_MAX); s.uptime = integer("uptime_ms", UINT32_MAX);
        s.leftTicks = integer("left_ticks", UINT32_MAX); s.rightTicks = integer("right_ticks", UINT32_MAX);
        s.route = integer("route_cmd_id", INT_MAX); s.index = integer("path_index", 31);
        s.coreSession = j.at("core_session_id").get<std::string>();
        s.from = j.at("from_node").get<std::string>(); s.to = j.at("to_node").get<std::string>();
        if (s.coreSession.empty() || s.coreSession.size() > 95 || s.from.size() > 7 || s.to.size() > 7)
            throw std::invalid_argument("Odometry identifier sai");
        s.tracking = boolean("tracking"); s.translating = boolean("translating"); s.reverse = boolean("reverse");
        s.distance = number("distance_cm", 100000); s.speed = number("speed_cm_s", 500);
        s.pulsesPer100Cm = number("pulses_per_100cm", 1000000, 0.1);
        if ((!s.tracking && (s.translating || s.reverse || s.route || !s.to.empty() || s.distance || s.speed)) ||
            (s.tracking && (!s.route || s.from.empty() || s.to.empty() || s.from == s.to)) ||
            (!s.translating && s.speed != 0)) throw std::invalid_argument("Odometry context sai");
        return s;
    }
};

struct EncoderObservation {
    using Clock = std::chrono::steady_clock;
    static constexpr int staleMs = 1000;
    uint32_t boot = 0, connection = 0, diagnosticUptime = 0;
    bool received = false;
    EncoderSample sample;
    Clock::time_point seen{};
    std::unordered_set<uint32_t> retiredBoots;
    bool bind(uint32_t nextBoot, uint32_t nextConnection, uint32_t uptime) {
        if (!nextBoot || !nextConnection || retiredBoots.count(nextBoot)) return false;
        if (boot == nextBoot && connection > nextConnection) return false;
        if (boot == nextBoot && connection == nextConnection &&
            static_cast<int32_t>(uptime - diagnosticUptime) < 0) return false;
        if (boot != nextBoot || connection != nextConnection) {
            if (boot && boot != nextBoot) retiredBoots.insert(boot);
            // Sixty-four boot changes require an explicit fresh Core session.
            if (retiredBoots.size() > 64) return false;
            boot = nextBoot; connection = nextConnection; received = false;
        }
        diagnosticUptime = uptime;
        return true;
    }
    bool accept(const EncoderSample& s) {
        if (s.boot != boot || s.connection != connection ||
            static_cast<int32_t>(s.uptime - diagnosticUptime) < -500) return false;
        if (received) {
            const uint32_t step = s.seq - sample.seq;
            if (!step || step >= 0x80000000u || static_cast<int32_t>(s.uptime - sample.uptime) < 0) return false;
            if (s.tracking && sample.tracking && s.route == sample.route && s.index == sample.index &&
                s.from == sample.from && s.to == sample.to && s.distance + 0.01 < sample.distance) return false;
        }
        sample = s; seen = Clock::now(); received = true;
        return true;
    }
};
