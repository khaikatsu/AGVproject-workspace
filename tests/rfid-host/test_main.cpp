#include "sensors/RFIDModule.h"
#include <cassert>
#include <cstdio>
#include <cstring>
uint32_t testNow=100;
int main() {
    RFIDModule reader;reader.begin();
    constexpr uint32_t poll=Config::RFID_POLL_INTERVAL_MS;
    assert(poll==20);
    RFIDFake::motorStopped=false;
    assert(reader.readCard()); // A brief GS2 presence must be delivered immediately.
    assert(std::strcmp(reader.event().node,"GS2")==0 && reader.event().newNode);
    assert(RFIDFake::halts==0 && RFIDFake::cryptoStops==0);
    RFIDFake::present=false;RFIDFake::motorStopped=true;reader.finishRead();
    assert(RFIDFake::halts==1 && RFIDFake::cryptoStops==1);
    ++testNow;assert(!reader.readCard());assert(RFIDFake::wakeCalls==1);
    testNow+=poll-2;assert(!reader.readCard());assert(RFIDFake::wakeCalls==1); // No poll before the configured interval.
    ++testNow;assert(!reader.readCard()); // At the interval a miss does one presence request.
    assert(RFIDFake::wakeCalls==2 && RFIDFake::requestCalls==0);
    testNow+=poll;RFIDFake::present=true;assert(reader.readCard());
    assert(!reader.event().newNode);reader.finishRead(); // Stationary HALTed tag is still read freshly.
    const int readsBefore=RFIDFake::wakeCalls;
    for (int i=0;i<50;++i) {
        testNow+=poll;assert(reader.readCard());
        assert(std::strcmp(reader.event().node,"GS2")==0 && !reader.event().newNode);
        reader.finishRead();
    }
    assert(RFIDFake::wakeCalls==readsBefore+50); // Repeated reads refresh age without advancing the route 50 times.
    testNow+=poll;RFIDFake::bytes[0]=0x97;RFIDFake::bytes[1]=0x46;RFIDFake::bytes[2]=0xBB;RFIDFake::bytes[3]=0x95;
    RFIDFake::motorStopped=false;assert(reader.readCard());assert(std::strcmp(reader.event().node,"K3")==0);
    assert(RFIDFake::halts==52);RFIDFake::motorStopped=true;reader.finishRead();
    testNow+=poll;RFIDFake::bytes[0]=0xAA;assert(reader.readCard());assert(!reader.event().node[0]);reader.finishRead();
    testNow+=poll;RFIDFake::collision=true;RFIDFake::bytes[0]=0x97;
    assert(reader.readCard());assert(std::strcmp(reader.event().node,"K3")==0);reader.finishRead();
    // A new tag appearing at 10 ms is delivered before the old 20 ms cadence.
    testNow+=poll;RFIDFake::collision=false;
    RFIDFake::bytes[0]=0x03;RFIDFake::bytes[1]=0x5F;RFIDFake::bytes[2]=0x25;RFIDFake::bytes[3]=0x39;
    RFIDFake::motorStopped=false;assert(reader.readCard());
    assert(reader.event().newNode && std::strcmp(reader.event().node,"GS1")==0);
    RFIDFake::present=false;RFIDFake::motorStopped=true;reader.finishRead();
    testNow+=poll;assert(!reader.readCard());
    std::puts("PASS RFID adapter: transient GS1/GS2 before HALT/motor stop, 20ms poll boundary, one wake probe, 50 stationary refreshes, unknown UID and collision");
}
