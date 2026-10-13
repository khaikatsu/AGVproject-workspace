#pragma once
#include "Arduino.h"
#include <stdexcept>
struct RFIDFake {
    inline static int wakeCalls=0,requestCalls=0,halts=0,cryptoStops=0;
    inline static bool present=true,serialOk=true,motorStopped=true,collision=false;
    inline static byte bytes[4]={0x97,0x77,0x02,0x96}; // GS2 from the configured table.
};
class MFRC522 {
public:
    enum StatusCode { STATUS_OK,STATUS_TIMEOUT,STATUS_COLLISION };
    struct Uid {byte size=4;byte uidByte[10]{};} uid;
    MFRC522(int,int) {}
    void PCD_Init() {}
    bool PICC_IsNewCardPresent() {++RFIDFake::requestCalls;return RFIDFake::present;}
    StatusCode PICC_WakeupA(byte*,byte*) {++RFIDFake::wakeCalls;return !RFIDFake::present?STATUS_TIMEOUT:RFIDFake::collision?STATUS_COLLISION:STATUS_OK;}
    bool PICC_ReadCardSerial() {for(int i=0;i<4;++i)uid.uidByte[i]=RFIDFake::bytes[i];return RFIDFake::serialOk;}
    void PICC_HaltA() {if(!RFIDFake::motorStopped)throw std::runtime_error("HALT before arrival motor stop");++RFIDFake::halts;}
    void PCD_StopCrypto1() {++RFIDFake::cryptoStops;}
};
