#pragma once

#include "Types.h"
#include "RouteMap.generated.h"
#include <cstdint>
#include <cstring>

namespace ClearanceMapCheck {
constexpr bool same(const char* a, const char* b) {
    while (*a && *b) { if (*a++ != *b++) return false; }
    return *a == *b;
}
constexpr const RouteMap::Node* node(const char* id) {
    for (const auto& n : RouteMap::NODES) if (same(n.id, id)) return &n;
    return nullptr;
}
constexpr int squared(const char* a, const char* b) {
    const auto* x = node(a); const auto* y = node(b);
    if (!x || !y) return -1;
    const int dx = x->x - y->x, dy = x->y - y->y;
    return dx * dx + dy * dy;
}
constexpr bool straight35(const char* a, const char* b, const char* c) {
    const auto* x = node(a); const auto* y = node(b); const auto* z = node(c);
    if (!x || !y || !z || squared(a, b) != 35 * 35 || squared(b, c) != 35 * 35)
        return false;
    const int ux = y->x - x->x, uy = y->y - x->y;
    const int vx = z->x - y->x, vy = z->y - y->y;
    return ux * vy == uy * vx && ux * vx + uy * vy > 0;
}
static_assert(squared("M1", "GS1") >= 50 * 50 && squared("M2", "C2") >= 50 * 50 &&
              squared("M3", "C3") >= 50 * 50 && squared("M4", "GS2") >= 50 * 50 &&
              squared("M5", "M6") >= 50 * 50 && squared("M6", "G6") >= 50 * 50,
              "Measured direct mutex exits changed in generated map");
static_assert(straight35("M1", "G2", "M5") && straight35("M4", "G5", "M6") &&
              straight35("M5", "G3", "M3") && straight35("M6", "G4", "M2"),
              "Measured 35+35 cm mutex exits changed in generated map");
}

// Physical measurements confirmed 09/10/2026: M boundary 10 cm, reader to
// rear 10 cm, RC522 read-ahead at most 8 cm. A 35 cm next tag leaves only
// 7 cm nominal clearance, so only the measured 50/100 cm exits or a verified
// straight 35+35 cm chain may release. No timeout or absent RFID is evidence.
class MutexClearance {
public:
    struct Release { char zone[NODE_TEXT_SIZE]{}; int32_t grantId = 0; };

    void reset() { for (auto& held : held_) held = Held{}; }

    bool canAcceptGrant(const char* zone, int32_t id) const {
        const int index = zoneIndex(zone);
        // A queued QoS 0 release has no server receipt ACK. The same token
        // must never authorize a second entry, even across WiFi reconnect.
        return index >= 0 && id > 0 && id != released_[index];
    }

    void grant(const char* zone, int32_t id, const char* current, bool localized) {
        const int index = zoneIndex(zone);
        if (index < 0 || !canAcceptGrant(zone, id)) return;
        Held& held = held_[index];
        if (held.id != id) held = Held{};
        held.id = id;
        if (localized && current && std::strcmp(current, zone) == 0) held.entered = true;
    }

    // Call only for a fresh, expected RFID in the active Core route.
    void reached(const char* from, const char* to) {
        if (!from || !to) return;
        for (int i = 0; i < 6; ++i) {
            Held& held = held_[i];
            if (!held.id) continue;
            const char zone[3] = {'M', static_cast<char>('1' + i), '\0'};
            if (std::strcmp(to, zone) == 0) {
                held.entered = true;
                held.stage = 0;
                continue;
            }
            if (!held.entered) continue;
            if (std::strcmp(from, zone) == 0) {
                held.stage = exitStage(i, to);
            } else if (held.stage == 1 && secondTag(i, from, to)) {
                held.stage = 2;
            } else if (held.stage != 2) {
                held.stage = 0;
            }
        }
    }

    bool pending(Release& out) const {
        for (int i = 0; i < 6; ++i) {
            const Held& held = held_[i];
            if (held.id > 0 && held.entered && held.stage == 2) {
                out.zone[0] = 'M'; out.zone[1] = static_cast<char>('1' + i);
                out.zone[2] = '\0'; out.grantId = held.id;
                return true;
            }
        }
        return false;
    }

    void queued(const Release& release) {
        const int index = zoneIndex(release.zone);
        if (index >= 0 && held_[index].id == release.grantId && held_[index].stage == 2) {
            released_[index] = release.grantId;
            held_[index] = Held{};
        }
    }

private:
    struct Held { int32_t id = 0; uint8_t stage = 0; bool entered = false; };
    Held held_[6]{};
    int32_t released_[6]{};
    static int zoneIndex(const char* zone) {
        return zone && zone[0] == 'M' && zone[1] >= '1' && zone[1] <= '6' && !zone[2]
            ? zone[1] - '1' : -1;
    }
    // 2 = measured direct exit >=50 cm; 1 = first 35 cm tag, still occupied.
    static uint8_t exitStage(int zone, const char* to) {
        static constexpr const char* direct[6][2] = {
            {"GS1", nullptr}, {"C2", nullptr}, {"C3", nullptr},
            {"GS2", nullptr}, {"M6", nullptr}, {"G6", nullptr}
        };
        static constexpr const char* shortExit[6] = {"G2", nullptr, nullptr, "G5", "G3", "G4"};
        for (const char* node : direct[zone])
            if (node && std::strcmp(to, node) == 0) return 2;
        return shortExit[zone] && std::strcmp(to, shortExit[zone]) == 0 ? 1 : 0;
    }
    static bool secondTag(int zone, const char* from, const char* to) {
        static constexpr const char* middle[6] = {"G2", nullptr, nullptr, "G5", "G3", "G4"};
        static constexpr const char* far[6] = {"M5", nullptr, nullptr, "M6", "M3", "M2"};
        return middle[zone] && std::strcmp(from, middle[zone]) == 0 &&
            std::strcmp(to, far[zone]) == 0;
    }
};
