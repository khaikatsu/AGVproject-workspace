#include "RFIDModule.h"
#include "core/ControlLogic.h"
#include <SPI.h>
#include <cstring>

bool RFIDModule::validUid(const char* uid) {
    const size_t length = std::strlen(uid);
    if (length != 8 && length != 14 && length != 20) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((uid[i] >= '0' && uid[i] <= '9') || (uid[i] >= 'A' && uid[i] <= 'F'))) return false;
    return true;
}
void RFIDModule::begin() {
    SPI.begin(Config::RFID_SCK_PIN, Config::RFID_MISO_PIN, Config::RFID_MOSI_PIN, Config::RFID_SS_PIN);
    reader_.PCD_Init();
    reader_.PCD_SetAntennaGain(MFRC522::RxGain_max);
    for (size_t i = 0; i < Config::RFID_NODES.size(); ++i) {
        const char* uid = Config::RFID_NODES[i].uid;
        if (!uid[0]) continue;
        if (!validUid(uid)) mappingsValid_ = false;
        for (size_t j = i + 1; j < Config::RFID_NODES.size(); ++j)
            if (std::strcmp(uid, Config::RFID_NODES[j].uid) == 0) mappingsValid_ = false;
    }
    if (!mappingsValid_ && Config::DEBUG_ENABLED) Serial.println("[RFID] Invalid/duplicate UID configuration");
}
const char* RFIDModule::getNodeFromUID(const char* uid) const {
    if (!mappingsValid_ || !uid || !validUid(uid)) return "";
    for (const auto& mapping : Config::RFID_NODES)
        if (std::strcmp(uid, mapping.uid) == 0) return mapping.node;
    return "";
}
bool RFIDModule::hasMapping(const char* node) const {
    if (!mappingsValid_) return false;
    for (const auto& mapping : Config::RFID_NODES)
        if (std::strcmp(node, mapping.node) == 0) return validUid(mapping.uid);
    return false;
}
bool RFIDModule::readCard() {
    event_.newNode = false;
    if (cardPending_) return false;
    if (static_cast<uint32_t>(millis() - lastPollMs_) < Config::RFID_POLL_INTERVAL_MS) return false;
    lastPollMs_ = millis();
    // WUPA covers both new and HALTed stationary tags. One presence transaction
    // avoids doing a REQA timeout followed by another WUPA timeout on every miss.
    byte atqa[2];
    byte atqaLength = sizeof(atqa);
    const auto presence = reader_.PICC_WakeupA(atqa, &atqaLength);
    if (presence != MFRC522::STATUS_OK && presence != MFRC522::STATUS_COLLISION) {
        if (!noCardSinceMs_) noCardSinceMs_ = millis();
        else if (static_cast<uint32_t>(millis() - noCardSinceMs_) >= 100) {
            lastNode_[0] = '\0';
        }
        return false;
    }
    noCardSinceMs_ = 0;
    if (!reader_.PICC_ReadCardSerial()) return false;
    if (reader_.uid.size != 4 && reader_.uid.size != 7 && reader_.uid.size != 10) {
        reader_.PICC_HaltA();
        reader_.PCD_StopCrypto1();
        return false;
    }
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < reader_.uid.size; ++i) {
        event_.uid[i * 2] = hex[reader_.uid.uidByte[i] >> 4];
        event_.uid[i * 2 + 1] = hex[reader_.uid.uidByte[i] & 15];
    }
    event_.uid[reader_.uid.size * 2] = '\0';
    AGVMath::copyText(event_.node, sizeof(event_.node), getNodeFromUID(event_.uid));
    event_.newNode = event_.node[0] && std::strcmp(event_.node, lastNode_) != 0;
    if (!event_.node[0]) lastNode_[0] = '\0';
    if (event_.newNode) AGVMath::copyText(lastNode_, sizeof(lastNode_), event_.node);
    cardPending_ = true;
    // Return the UID before HALT/logging: the caller can stop the motor first.
    return true;
}
void RFIDModule::finishRead() {
    if (!cardPending_) return;
    reader_.PICC_HaltA();
    reader_.PCD_StopCrypto1();
    cardPending_ = false;
    const uint32_t now = millis();
    if (Config::DEBUG_ENABLED && (event_.newNode || std::strcmp(lastLoggedUid_, event_.uid) != 0 ||
                                 static_cast<uint32_t>(now-lastLogMs_) >= 1000)) {
        Serial.printf("[RFID] UID=%s node=%s new=%d\n", event_.uid, event_.node, event_.newNode);
        AGVMath::copyText(lastLoggedUid_, sizeof(lastLoggedUid_), event_.uid);
        lastLogMs_ = now;
    }
}
