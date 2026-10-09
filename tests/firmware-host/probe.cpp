// Bien dich truc tiep .ino va ArduinoJson that; I/O WiFi/Serial/MQTT duoc adapter host.
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include "../../src/json.hpp"
#include "../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino"
using json = nlohmann::json;
static void receive(const std::string& topic,const std::string& payload) {
    std::vector<char> name(topic.begin(),topic.end());name.push_back(0);
    std::vector<byte> bytes(payload.begin(),payload.end());
    onMqttMessage(name.data(),bytes.data(),static_cast<unsigned int>(bytes.size()));
    processCommand();processGrant();
}
static void serial(const std::string& line) {
    std::vector<char> buffer(line.begin(),line.end());buffer.push_back(0);handleSerialLine(buffer.data());
}
static void scan(const std::string& input) {
    char uid[UID_BYTES]={};
    if (!normalizeUid(input.c_str(),uid)) {onUidRead(input.c_str());return;}
    std::vector<byte> bytes;
    for (size_t i=0;i<strlen(uid);i+=2) bytes.push_back((byte)std::stoul(std::string(uid+i,2),nullptr,16));
    hostCards.push_back(bytes);hostMillis+=20;pollRfid();
}
static void observe(const std::string& node,const std::string& uid) {serial("learn "+node);scan(uid);}
static json stats() {
    return {{"accepted",acceptedCommands},{"duplicates",duplicateCommands},{"last_id",highestCommandId},
        {"node",currentNode},{"battery",batteryPercent},{"held_zones",grantedZones},
        {"position_known",positionKnown},{"mode",modeName()},{"simulate",simulationEnabled},{"home",homeStation()}};
}
static void selfTest(const char* mapFile,const char* fixtureFile,const char* tagFile) {
    assert(!positionKnown && !*currentNode && !simulationEnabled);
    assert((hostSpiPins==std::array<int,4>{18,19,23,5}));
    assert(!AgvDrive::motorRunning());
    serial("motor arm");assert(!segmentArmed && !AgvDrive::motorRunning());
    std::ifstream tagInput(tagFile);json suppliedTags;tagInput>>suppliedTags;
    assert(suppliedTags.size()==MAP_NODE_COUNT);
    for (auto it=suppliedTags.begin();it!=suppliedTags.end();++it) {
      const int index=nodeIndex(it.key().c_str());assert(index>=0);
      assert(!strcmp(tagUids[index],it.value().get<std::string>().c_str()));
    }
    hostNvs["agv-rfid/M2"]="DEADBEEF"; // stale mapping from the preloaded-table firmware
    initRfid();assert(!strcmp(tagUids[nodeIndex("M2")],"072F2495"));
    hostNvs.erase("agv-rfid/M2");
    scan("77B04295");assert(positionKnown && !strcmp(currentNode,"S1"));
    scan("D7E21F95");assert(positionKnown && !strcmp(currentNode,"S2"));
    initRfid();assert(!positionKnown); // UID table exists but boot position remains UNKNOWN.
    sendTelemetry();assert(hostPublishes.back().payload=="UNKNOWN|85|0");
    receive(commandTopic,R"({"cmd":"GOTO","cmd_id":90,"path":["K1"],"target":"K1"})");assert(acceptedCommands==0);
    receive(commandTopic,R"({"cmd":"CHARGE_START","cmd_id":91})");assert(acceptedCommands==0);
    serial("node S1");assert(!positionKnown);
    observe("S1","01020304");assert(positionKnown && !strcmp(currentNode,"S1"));
    assert(hostNvs["agv-rfid/uid-S1"]=="01020304");
    receive(commandTopic,R"({"cmd":"CHARGE_START","cmd_id":92})");assert(receiverMode==CHARGE_TEST);
    serial("charged 90");assert(receiverMode==READY_TEST && hostPublishes.back().payload=="S1|90|0");
    const auto afterCharged=hostPublishes.size();serial("charged 90");assert(hostPublishes.size()==afterCharged);
    serial("battery 85");
    // UID mappings survive reinitialization; measured boot position does not.
    initRfid();assert(!positionKnown && !*currentNode && !strcmp(tagUids[nodeIndex("S1")],"01020304"));
    scan("01020304");assert(positionKnown && !strcmp(currentNode,"S1"));
    serial("learn S2");scan("01020304");assert(!strcmp(tagUids[nodeIndex("S2")],"D7E21F95") && !strcmp(currentNode,"S1"));
    hostNvsWriteSucceeds=false;observe("G1","11223344");assert(!strcmp(tagUids[nodeIndex("G1")],"37F61F95") && !positionKnown);
    hostNvsWriteSucceeds=true;observe("G1","11223344");assert(!strcmp(currentNode,"G1"));
    assert(hostHaltCount>0 && hostHaltCount==hostStopCryptoCount);
    acceptedCommands=0;duplicateCommands=0;highestCommandId=0;historyNext=0;
    memset(history,0,sizeof(history));lastCommand=ReceivedCommand{};hostPublishes.clear();
    std::ifstream mapInput(mapFile);json map;mapInput>>map;
    std::vector<std::string> expected,actual;
    for (const auto& node:map["nodes"]) expected.push_back(node["id"]);
    for (const char* node:MAP_NODES) actual.push_back(node);
    std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());assert(expected==actual);
    std::ifstream fixtureInput(fixtureFile);json fixture;fixtureInput>>fixture;
    for (const auto& message:fixture["messages"]) if (message["direction"]=="core_to_robot" && message["topic"].get<std::string>().find("/command")!=std::string::npos) {
        ReceivedCommand parsed{};const std::string payload=message["payload"].dump();
        assert(parseCommand(payload.c_str(),payload.size(),parsed));
    }
    const std::string go=R"({"cmd":"GOTO","cmd_id":100,"path":["M5","M6","G4","K4"],"target":"K4"})";
    receive(commandTopic,go);assert(acceptedCommands==1 && !strcmp(currentNode,"G1"));
    assert(!hasZoneGrantsForNextSegment()); // G1 -> M5 needs M5 grant
    grantedZones=(1<<4);assert(hasZoneGrantsForNextSegment());
    strcpy(currentNode,"M5");lastCommand.path[routeCursor][0]='M';lastCommand.path[routeCursor][1]='6';lastCommand.path[routeCursor][2]=0;
    assert(!hasZoneGrantsForNextSegment()); // M5 -> M6 needs both tokens
    grantedZones|=1<<5;assert(hasZoneGrantsForNextSegment());
    strcpy(currentNode,"G1");strcpy(lastCommand.path[routeCursor],"M5");grantedZones=0;
    assert(hostPublishes.back().topic==ackTopic && hostPublishes.back().payload=="100" && !hostPublishes.back().retained);
    receive(commandTopic,go);assert(acceptedCommands==1 && duplicateCommands==1);
    const auto count=hostPublishes.size();
    receive(commandTopic,R"({"cmd":"STOP","cmd_id":100})");assert(hostPublishes.size()==count && acceptedCommands==1);
    for (const char* bad:{"{}","[]","{broken}",
        "{\"cmd\":\"STOP\",\"cmd_id\":0}","{\"cmd\":\"STOP\",\"cmd_id\":2147483648}",
        "{\"cmd\":\"STOP\",\"cmd_id\":\"101\"}","{\"cmd\":\"UNKNOWN\",\"cmd_id\":101}",
        "{\"cmd\":\"GOTO\",\"cmd_id\":101,\"target\":\"K4\",\"path\":[\"KHO_4\"]}",
        "{\"cmd\":\"GOTO\",\"cmd_id\":101,\"target\":\"K4\",\"path\":[\"K3\"]}"}) {
        receive(commandTopic,bad);assert(hostPublishes.size()==count && acceptedCommands==1);
    }
    receive(commandTopic,std::string(MAX_COMMAND_BYTES+1,'x'));assert(acceptedCommands==1);
    std::string nul=go;nul[5]=0;receive(commandTopic,nul);assert(acceptedCommands==1);
    json longRoute={{"cmd","GOTO"},{"cmd_id",101},{"target","K4"},{"path",json::array()}};
    for (size_t i=0;i<MAX_PATH_NODES;++i) longRoute["path"].push_back("M5");
    longRoute["path"].push_back("K4");receive(commandTopic,longRoute.dump());assert(acceptedCommands==1);
    serial("ack off");receive(commandTopic,R"({"cmd":"STOP","cmd_id":101})");
    assert(acceptedCommands==2 && hostPublishes.size()==count);
    serial("ack on");receive(commandTopic,R"({"cmd":"STOP","cmd_id":101})");
    assert(acceptedCommands==2 && duplicateCommands==2 && hostPublishes.back().payload=="101");
    mqtt.publishSucceeds=false;receive(commandTopic,R"({"cmd":"WAIT","cmd_id":102})");assert(pendingAckId==102);
    mqtt.publishSucceeds=true;loop();assert(pendingAckId==0 && acceptedCommands==3);
    wasConnected=true;mqtt.connectedFlag=false;loop();assert(!positionKnown && receiverMode==PAUSED_LINK);mqtt.connectedFlag=true;
    receive(commandTopic,R"({"cmd":"WAIT","cmd_id":102})");assert(acceptedCommands==3);
    receive(commandTopic,go);assert(acceptedCommands==3 && lastCommand.kind==WAIT_COMMAND);
    hostMillis+=250;pollRfid();assert(positionKnown && !strcmp(currentNode,"G1")); // xác minh lại thẻ HALT vẫn ở trạm/đầu đọc
    receive(grantTopic,R"({"zone":"M5","grant_id":500})");assert(grantedZones==0);
    serial("request M5");receive(grantTopic,R"({"zone":"M5","grant_id":500})");assert(grantedZones==(1<<4));
    assert(hostPublishes.back().topic==mutexAckTopic);
    const auto grantedCount=hostPublishes.size();receive(grantTopic,R"({"zone":"M5","grant_id":499})");assert(hostPublishes.size()==grantedCount);
    receive(grantTopic,R"({"zone":"M5","grant_id":500})");assert(hostPublishes.size()==grantedCount+1);
    observe("M6","22334455");serial("release M5");assert(grantedZones==0 && json::parse(hostPublishes.back().payload)["grant_id"]==500);
    serial("node KHO_4");assert(!strcmp(currentNode,"M6"));
    serial("battery 101");assert(batteryPercent==85);
    serial("telemetry off");const auto beforeOff=hostPublishes.size();sendTelemetry();assert(hostPublishes.size()==beforeOff);
    serial("telemetry on");assert(hostPublishes.back().payload=="M6|85|0");
    serial("simulate on");assert(!positionKnown);serial("node K4");assert(positionKnown && !strcmp(currentNode,"K4"));
    serial("simulate off");assert(!positionKnown);scan("22334455");assert(positionKnown && !strcmp(currentNode,"M6"));
    scan("AABBCCDD");assert(!positionKnown && hostPublishes.back().payload=="UNKNOWN|85|0");
    const auto unknownCount=hostPublishes.size();hostMillis+=250;pollRfid();assert(hostPublishes.size()==unknownCount); // không flood thẻ lạ đứng yên
    observe("S2","10203040506070");receive(commandTopic,R"({"cmd":"CHARGE_START","cmd_id":103})");assert(acceptedCommands==3); // 01 không sạc S2
    const std::string arrive=R"({"cmd":"GOTO","cmd_id":103,"path":["K1"],"target":"K1"})";
    receive(commandTopic,arrive);assert(receiverMode==ROUTE_TEST && !strcmp(currentNode,"S2"));
    observe("K1","102030405060708090A0");assert(positionKnown && receiverMode==READY_TEST);
    receive(commandTopic,arrive);assert(receiverMode==READY_TEST && acceptedCommands==4); // retry sau đến đích không khởi động lại route
    serial("forget K1");assert(!positionKnown && !strcmp(tagUids[nodeIndex("K1")],"57D42295"));
    observe("G1","11223344");
    receive(commandTopic,R"({"cmd":"GOTO","cmd_id":104,"path":["K1"],"target":"K1"})");assert(motionAllowed);
    hostMillis+=CONTROLLER_TIMEOUT_MS+1;loop();assert(!motionAllowed && receiverMode==PAUSED_LINK);
    receive(heartbeatTopic,"session-one");observe("G1","11223344");
    receive(commandTopic,R"({"cmd":"GOTO","cmd_id":105,"path":["K1"],"target":"K1"})");assert(motionAllowed);
    receive(heartbeatTopic,"session-two");assert(!motionAllowed && !positionKnown);
    scan("11223344");
    receive(commandTopic,R"({"cmd":"GOTO","cmd_id":106,"path":["M5","K4"],"target":"K4"})");
    assert(motionAllowed && receiverMode==ROUTE_TEST);
    scan("01020304"); // valid RFID but not the next node in this path
    assert(!motionAllowed && failsafeLatched && !segmentArmed && !AgvDrive::motorRunning());
    assert(!strcmp(currentNode,"S1") && receiverMode==PAUSED_LINK);
    serial("learn S1");serial("learn cancel");assert(!*learningNode);
    std::cout<<"PASS sketch RC522 26 preloaded UIDs, NVS override/legacy isolation, UNKNOWN boot, command ACK/dedup, reconnect/mutex and map synchronization\n";
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--robot") ROBOT_ID=argv[2];
    setup();
    if (argc==5 && std::string(argv[1])=="--self-test") {selfTest(argv[2],argv[3],argv[4]);return 0;}
    hostPublishHook=[](const HostPublish& event) {
        std::cout<<json{{"type","publish"},{"topic",event.topic},{"payload",event.payload},{"retained",event.retained}}.dump()<<std::endl;
    };
    std::string line;
    while (std::getline(std::cin,line)) {
        try {
            const json event=json::parse(line);const std::string type=event.at("type");
            if (type=="mqtt") receive(event.at("topic"),event.at("payload"));
            else if (type=="serial") serial(event.at("line"));
            else if (type=="rfid") scan(event.at("uid"));
            else if (type=="rfid_reset") initRfid();
            else if (type=="telemetry") sendTelemetry();
            else if (type!="stats") throw std::runtime_error("Unknown probe event");
            json result=stats();result["type"]="stats";result["request_id"]=event.value("request_id",0);
            std::cout<<result.dump()<<std::endl;
        } catch (const std::exception& error) {std::cerr<<error.what()<<std::endl;return 1;}
    }
}
