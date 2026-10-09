#pragma once
#include <deque>
#include <vector>
#include <algorithm>
inline std::deque<std::vector<byte>> hostCards;
inline byte hostReaderVersion=0x92;
inline int hostHaltCount=0,hostStopCryptoCount=0;
inline std::vector<byte> hostCardInField;
inline bool hostHalted=false;
class MFRC522 {
public:
    enum {VersionReg=0x37};
    enum StatusCode {STATUS_OK,STATUS_TIMEOUT};
    struct {byte size=0;byte uidByte[10]={};} uid;
    MFRC522(byte,byte) {}
    void PCD_Init() {}
    byte PCD_ReadRegister(int) {return hostReaderVersion;}
    bool PICC_IsNewCardPresent() {return !hostCards.empty();}
    bool PICC_ReadCardSerial() {
        if (hostCards.empty()) return false;
        const auto bytes=hostCards.front();hostCards.pop_front();hostCardInField=bytes;hostHalted=false;uid.size=(byte)bytes.size();
        std::copy_n(bytes.begin(),std::min<size_t>(10,bytes.size()),uid.uidByte);return true;
    }
    StatusCode PICC_WakeupA(byte*,byte*) {
        if (!hostHalted || hostCardInField.empty()) return STATUS_TIMEOUT;
        hostCards.push_back(hostCardInField);hostHalted=false;return STATUS_OK;
    }
    void PICC_HaltA() {++hostHaltCount;hostHalted=true;}
    void PCD_StopCrypto1() {++hostStopCryptoCount;}
};
