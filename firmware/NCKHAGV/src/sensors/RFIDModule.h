#pragma once
#include <Arduino.h>
#include <MFRC522.h>
#include "Types.h"

class RFIDModule {
public:
    void begin();
    void update() { if (readCard()) finishRead(); }
    bool readCard();
    // Call after the controller has stopped/applied the arrival event.
    void finishRead();
    const char* getUID() const { return event_.uid; }
    const char* getNodeFromUID(const char* uid) const;
    void clearLastNode() { lastNode_[0] = '\0'; }
    bool isNewNode() const { return event_.newNode; }
    bool hasMapping(const char* node) const;
    const RFIDEvent& event() const { return event_; }
private:
    static bool validUid(const char* uid);
    MFRC522 reader_{Config::RFID_SS_PIN, Config::RFID_RST_PIN};
    RFIDEvent event_{};
    char lastNode_[NODE_TEXT_SIZE]{};
    uint32_t lastPollMs_ = 0;
    uint32_t noCardSinceMs_ = 0;
    bool mappingsValid_ = true;
    bool cardPending_ = false;
    uint32_t lastLogMs_ = 0;
    char lastLoggedUid_[UID_TEXT_SIZE]{};
};
