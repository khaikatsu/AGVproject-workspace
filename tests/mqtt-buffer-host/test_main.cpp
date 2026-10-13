// Link the installed PubSubClient 2.8 source to exercise its actual shared RX/TX buffer.
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include "core/CoreProtocol.h"
#include <cassert>
#include <deque>
#include <iostream>
#include <string>
class Wire:public Client {
public:
    std::deque<uint8_t> input{0x20,0x02,0x00,0x00};bool online=true;
    int connect(const char*,uint16_t) override{return 1;}int connect(IPAddress,uint16_t) override{return 1;}
    uint8_t connected() override{return online;}void stop() override{online=false;}void flush() override{}
    int available() override{return static_cast<int>(input.size());}
    int read() override{auto v=input.front();input.pop_front();return v;}
    size_t write(uint8_t) override{return 1;}size_t write(const uint8_t*,size_t n) override{return n;}
    void grant(const std::string& payload){
        const std::string topic="agv/01/mutex_grant";const size_t len=2+topic.size()+payload.size();assert(len<128);
        input.push_back(0x30);input.push_back(static_cast<uint8_t>(len));input.push_back(0);input.push_back(topic.size());
        for(char c:topic)input.push_back(c);for(char c:payload)input.push_back(c);
    }
};
static PubSubClient* client;static bool regression=false,changed=false;static int accepted=0;
static void receive(char*,uint8_t* payload,unsigned int size){
    if(regression){
        StaticJsonDocument<256> doc;assert(!deserializeJson(doc,payload,size));
        const char* zone=doc["zone"];const std::string before=zone;const auto offset=zone-reinterpret_cast<char*>(payload);
        assert(offset>=0 && static_cast<size_t>(offset)<size);
        char ack[96];std::snprintf(ack,sizeof(ack),"{\"zone\":\"%s\",\"grant_id\":57360}",zone);
        assert(client->publish("agv/01/mutex_ack",ack,false));
        const std::string after(zone,strnlen(zone,size-static_cast<size_t>(offset)));
        changed=after!=before;assert(after.size()>=NODE_TEXT_SIZE);
        std::cout<<"REPRO old RX alias: zone before="<<before<<", after ACK="<<after<<"\n";
        // Do not execute the old overflowing strcpy into CoreGrant.zone.
    }else{
        CoreGrant grant;assert(CoreProtocol::decodeGrant(payload,size,grant));char ack[96];
        assert(CoreProtocol::grantAck(grant,ack,sizeof(ack)));assert(client->publish("agv/01/mutex_ack",ack,false));
        assert(std::strcmp(grant.zone,"M2")==0 && grant.grantId==57360);++accepted;
    }
}
int main(){
    Wire wire;PubSubClient mqtt(wire);client=&mqtt;mqtt.setBufferSize(1792);mqtt.setCallback(receive);assert(mqtt.connect("fixture"));
    regression=true;wire.grant(R"({"zone":"M2","grant_id":57360})");assert(mqtt.loop());assert(changed);
    regression=false;
    for(int i=0;i<1000;++i){wire.grant(i%2 ? R"({"grant_id":57360,"zone":"M2"})" : R"({"zone":"M2","grant_id":57360})");assert(mqtt.loop());}
    assert(accepted==1000);std::cout<<"PASS actual PubSubClient: owned grant survives 1000 ACK callbacks and field reorder\n";
}
