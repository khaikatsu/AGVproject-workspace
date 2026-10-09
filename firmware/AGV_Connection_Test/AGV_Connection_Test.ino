/*
  ESP32 DevKit / Arduino IDE: test hop dong MQTT v0 cua AGVproject.
  Libraries: PubSubClient 2.8, ArduinoJson 7.x, MFRC522 1.4.x. Serial: 115200, Newline.
  Sua WIFI_SSID, WIFI_PASSWORD, MQTT_HOST; moi xe co ROBOT_ID rieng.
  Vi tri doc RC522; boot cho RFID, learn UID luu NVS. Pin/vat can/sac la TEST.
  Sketch chi nhan/in lenh, KHONG chay motor/relay sac.
  GOTO duoc ACK sau khi validate/luu; KHONG tu bao den dich.
  Xem firmware/AGV_Connection_Test/README.md.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include "Config.h"
#include "TestConfig.h"
#include "driver.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ===== CAU HINH CAN SUA =====
const char* WIFI_SSID = "DESKTOP-KEI72OV 5054";
const char* WIFI_PASSWORD = "t<8A9513";
const char* MQTT_HOST = "192.168.137.1"; // Mobile Hotspot cua laptop; doi neu laptop khong phat hotspot.
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";           // De rong neu broker test cho anonymous.
const char* MQTT_PASSWORD = "";
const char* ROBOT_ID = "01";          // Topic agv/01/...; xe thu hai dung "02".
// RC522 da chot: nguon 3.3V, SDA la chan SPI SS (khong phai I2C).
const uint8_t RFID_SCK = 18, RFID_MISO = 19, RFID_MOSI = 23, RFID_SS = Config::RFID_SS_PIN, RFID_RST = 22;

const uint32_t TELEMETRY_INTERVAL_MS = 1000; // Core offline sau >10 giay.
const uint32_t MQTT_RETRY_MS = 3000;
const bool MQTT_USE_TLS = false; // Bat khi da co broker TLS va CA; khong setInsecure().
const char* MQTT_CA_PEM = ""; // PEM CA, chung chi broker phai khop MQTT_HOST.
const uint32_t CONTROLLER_TIMEOUT_MS = 3000;
const uint32_t WIFI_RETRY_MS = 10000;
const size_t MAX_COMMAND_BYTES = 2048;
const uint16_t MQTT_BUFFER_BYTES = 2304; // Bao gom header/topic, khong chi JSON.
const size_t MAX_PATH_NODES = 64;
const size_t NODE_BYTES = 8; // Du cho cac ID map v3; map moi can dong bo lai.
const size_t HISTORY_SIZE = 8;
const char* MAP_NODES[] = {"C1","C2","C3","C4","M1","M2","M3","M4","M5","M6",
  "G1","G2","G3","G4","G5","G6","K1","K2","K3","K4","K5","K6","GS1","GS2","S1","S2"};
struct TagBinding { const char* node; const char* uid; };
// Danh sach 26 UID do nguoi dung cung cap ngay 03/10/2026; ca hai xe dung chung.
const TagBinding DEFAULT_TAGS[] = {
  {"C1","B7BC2A95"}, {"C2","B771B795"}, {"C3","A79A0695"}, {"C4","575A1995"},
  {"M1","579E8495"}, {"M2","072F2495"}, {"M3","27CE0C95"}, {"M4","D78A3395"},
  {"M5","17693195"}, {"M6","F7354695"},
  {"G1","37F61F95"}, {"G2","B7A5BD95"}, {"G3","A7520C95"}, {"G4","27582F95"},
  {"G5","E7CDB995"}, {"G6","37869F95"},
  {"K1","57D42295"}, {"K2","C3008837"}, {"K3","9746BB95"}, {"K4","27EB4095"},
  {"K5","A75E1595"}, {"K6","47683395"},
  {"GS1","035F2539"}, {"GS2","97770296"}, {"S1","77B04295"}, {"S2","D7E21F95"}
};
const char* ZONES[] = {"M1","M2","M3","M4","M5","M6"};
const size_t MAP_NODE_COUNT = sizeof(MAP_NODES)/sizeof(MAP_NODES[0]);
static_assert(sizeof(DEFAULT_TAGS)/sizeof(DEFAULT_TAGS[0]) == MAP_NODE_COUNT,
              "Moi node tren map can mot UID mac dinh");
const size_t UID_BYTES = 21; // UID 4/7/10 byte -> HEX 8/14/20 ky tu + NUL.
enum ReceiverMode { AWAIT_RFID, READY_TEST, ROUTE_TEST, CHARGE_TEST, STOPPED_TEST, PAUSED_LINK };
ReceiverMode receiverMode = AWAIT_RFID;
bool positionKnown = false, simulationEnabled = false, nvsReady = false, readerAvailable = false;
bool rfidRefreshRequested = true;
char tagUids[MAP_NODE_COUNT][UID_BYTES] = {}, learningNode[NODE_BYTES] = {};
uint8_t routeCursor = 0;
uint32_t lastRfidPollMs = 0;
uint32_t lastRfidWakeMs = 0;
Preferences tagStorage;
MFRC522 reader(RFID_SS,RFID_RST);

enum CommandKind { GOTO_COMMAND, STOP_COMMAND, WAIT_COMMAND, CHARGE_COMMAND };
struct ReceivedCommand {
  int32_t id;
  CommandKind kind;
  uint8_t pathCount;
  char target[NODE_BYTES];
  char path[MAX_PATH_NODES][NODE_BYTES];
};

WiFiClient transport;
WiFiClientSecure secureTransport;
PubSubClient mqtt(transport);
char commandTopic[64], telemetryTopic[64], ackTopic[64], grantTopic[64];
char requestTopic[64], releaseTopic[64], errorTopic[64], chargedTopic[64];
char clientId[64], currentNode[NODE_BYTES];
char receivedJson[MAX_COMMAND_BYTES+1], receivedGrant[256];
char mutexAckTopic[64],statusTopic[64],controllerSession[96] = {};
const char* heartbeatTopic = "agv/server/heartbeat";
int32_t grantIds[6] = {};
uint32_t lastControllerSeenMs = 0;
uint32_t lastMutexRequestMs = 0;
bool motionAllowed = false;
bool segmentArmed = false;
bool failsafeLatched = false;
size_t receivedLength = 0;
bool commandPending = false, grantPending = false;
bool ackEnabled = true, telemetryEnabled = true, configured = false;
bool wasConnected = false;
uint8_t batteryPercent = 85, obstacleFlag = 0, requestedZones = 0, grantedZones = 0;
uint32_t lastTelemetryMs = 0, lastMqttTryMs = 0, lastWifiTryMs = 0;
int32_t highestCommandId = 0, pendingAckId = 0;
uint32_t acceptedCommands = 0, duplicateCommands = 0;
ReceivedCommand history[HISTORY_SIZE] = {}, lastCommand = {};
size_t historyNext = 0;
char serialLine[160];
size_t serialLength = 0;
bool serialOverflow = false;

bool knownNode(const char* node) {
  if (!node) return false;
  for (const char* item : MAP_NODES) if (!strcmp(item,node)) return true;
  return false;
}
const char* homeStation() {
  if (!strcmp(ROBOT_ID,"01")) return "S1";
  if (!strcmp(ROBOT_ID,"02")) return "S2";
  return "";
}
const char* modeName() {
  const char* names[] = {"AWAIT_RFID","READY_TEST","ROUTE_TEST","CHARGE_TEST","STOPPED_TEST","PAUSED_LINK"};
  return names[receiverMode];
}
int nodeIndex(const char* node) {
  if (node) for (size_t i=0;i<MAP_NODE_COUNT;++i) if (!strcmp(MAP_NODES[i],node)) return (int)i;
  return -1;
}
const char* defaultUidForNode(const char* node) {
  if (node) for (const TagBinding& binding : DEFAULT_TAGS)
    if (!strcmp(binding.node,node)) return binding.uid;
  return "";
}
bool normalizeUid(const char* input,char* output) {
  if (!input) return false;
  const size_t length = strlen(input);
  if (length!=8 && length!=14 && length!=20) return false;
  for (size_t i=0;i<length;++i) {
    if (!isxdigit((unsigned char)input[i])) return false;
    output[i]=(char)toupper((unsigned char)input[i]);
  }
  output[length]=0;return true;
}
// Forward declarations keep this one-file sketch usable in both Arduino and host tests.
void sendTelemetry();
void invalidatePosition(const char* reason);
bool publishText(const char* topic,const char* payload);
void enterFailsafe(const char* reason) {
  motionAllowed = false;failsafeLatched = true;commandPending = false;grantPending = false;
  segmentArmed = false;AgvDrive::stop();
  invalidatePosition(reason);receiverMode = PAUSED_LINK;
}
void invalidatePosition(const char* reason) {
  motionAllowed = false;segmentArmed = false;AgvDrive::stop();
  positionKnown=false;receiverMode=AWAIT_RFID;rfidRefreshRequested=true;
  Serial.printf("[POSITION UNKNOWN] %s; cho quet the moi, giu quyen mutex\n",reason);
  sendTelemetry();
}
void observeNode(const char* node,const char* source) {
  if (!knownNode(node)) return;
  if (receiverMode==ROUTE_TEST && !strcmp(node,currentNode)) return; // RC522 may reread the same stationary tag.
  if (receiverMode==ROUTE_TEST && (routeCursor>=lastCommand.pathCount ||
      strcmp(node,lastCommand.path[routeCursor]))) {
    motionAllowed=false;segmentArmed=false;failsafeLatched=true;receiverMode=PAUSED_LINK;
    AgvDrive::stop();
    publishText(errorTopic,"RFID_UNEXPECTED_NODE");
    Serial.printf("[ROUTE ERROR] RFID %s khac node du kien\n",node);
  }
  segmentArmed=false;AgvDrive::stop(); // each RFID is a supervised segment boundary.
  strcpy(currentNode,node);positionKnown=true;
  if (failsafeLatched) receiverMode=PAUSED_LINK;
  else if (receiverMode==AWAIT_RFID || receiverMode==PAUSED_LINK) receiverMode=READY_TEST;
  Serial.printf("[POSITION %s] %s | home=%s\n",source,currentNode,homeStation());
  if (receiverMode==ROUTE_TEST) {
    if (routeCursor<lastCommand.pathCount && !strcmp(lastCommand.path[routeCursor],node)) ++routeCursor;
    if (!strcmp(node,lastCommand.target)) {
      motionAllowed = false;segmentArmed=false;AgvDrive::stop();
      receiverMode=READY_TEST;
      Serial.println("[AT TARGET] Da gui RFID; cho lenh moi tu core, khong tu di tiep.");
    }
  }
  sendTelemetry();
}
void initRfid() {
  // Static map is available on both boards. NVS only stores explicit local overrides.
  // No boot position is restored: the RC522 must scan a fresh tag each start.
  currentNode[0]=0;positionKnown=false;receiverMode=AWAIT_RFID;learningNode[0]=0;rfidRefreshRequested=true;
  memset(tagUids,0,sizeof(tagUids));
  for (const TagBinding& binding : DEFAULT_TAGS) {
    const int index=nodeIndex(binding.node);
    if (index>=0) strcpy(tagUids[index],binding.uid);
  }
  if (nvsReady) tagStorage.end();
  nvsReady=tagStorage.begin("agv-rfid",false);
  if (!nvsReady) Serial.println("[NVS ERROR] Khong mo duoc UID storage; dung bang UID co san, learn bi tu choi.");
  else {
    char loaded[MAP_NODE_COUNT][UID_BYTES] = {};
    memcpy(loaded,tagUids,sizeof(loaded));
    bool legacyFound=false,invalidOverride=false;
    for (size_t i=0;i<MAP_NODE_COUNT;++i) {
      if (tagStorage.getString(MAP_NODES[i],"").length()) legacyFound=true;
      char key[16];snprintf(key,sizeof(key),"uid-%s",MAP_NODES[i]);
      const auto saved=tagStorage.getString(key,"");
      if (!saved.length()) continue;
      char uid[UID_BYTES]={};
      if (!normalizeUid(saved.c_str(),uid)) {invalidOverride=true;break;}
      strcpy(loaded[i],uid);
    }
    if (!invalidOverride) for (size_t i=0;i<MAP_NODE_COUNT && !invalidOverride;++i)
      for (size_t j=i+1;j<MAP_NODE_COUNT;++j)
        if (!strcmp(loaded[i],loaded[j])) {invalidOverride=true;break;}
    if (invalidOverride) Serial.println("[NVS ERROR] Override UID sai/trung; dung bang 26 UID co san.");
    else memcpy(tagUids,loaded,sizeof(loaded));
    if (legacyFound) Serial.println("[RFID] Mapping NVS cu duoc bo qua; 26 UID moi da co trong firmware.");
  }
  Serial.printf("[RFID MAP] %u UID mac dinh, override qua learn neu can\n",unsigned(MAP_NODE_COUNT));
  SPI.begin(RFID_SCK,RFID_MISO,RFID_MOSI,RFID_SS);reader.PCD_Init();
  const byte version=reader.PCD_ReadRegister(MFRC522::VersionReg);
  readerAvailable=version!=0x00 && version!=0xFF;
  Serial.printf("[RC522] version=0x%02X SCK=%u MISO=%u MOSI=%u SS=%u RST=%u\n",
    version,RFID_SCK,RFID_MISO,RFID_MOSI,RFID_SS,RFID_RST);
  if (!readerAvailable) Serial.println("[RC522 ERROR] Kiem tra 3.3V/GND/SPI. MQTT van test duoc; vi tri chua xac minh.");
  Serial.println("[BOOT] Cho quet RFID moi. Chi luu UID, khong khoi phuc vi tri cu.");
}
void onUidRead(const char* input) {
  char uid[UID_BYTES]={};
  if (!normalizeUid(input,uid)) {invalidatePosition("UID format sai");return;}
  Serial.printf("[RFID UID] %s\n",uid);
  if (simulationEnabled) {Serial.println("[RFID IGNORED] simulate on; dung simulate off de doc vi tri that");return;}
  if (*learningNode) {
    const int destination=nodeIndex(learningNode);bool collision=false;
    for (size_t i=0;i<MAP_NODE_COUNT;++i) if ((int)i!=destination && !strcmp(tagUids[i],uid)) collision=true;
    if (collision) Serial.println("[LEARN REJECT] UID da gan node khac; khong ghi de.");
    else if (nvsReady && destination>=0) {
      char key[16];snprintf(key,sizeof(key),"uid-%s",learningNode);
      if (tagStorage.putString(key,uid)!=strlen(uid)) {
        Serial.println("[LEARN FAILED] NVS khong ghi duoc; chua luu UID.");
      } else {
      strcpy(tagUids[destination],uid);Serial.printf("[LEARN SAVED] %s -> %s\n",learningNode,uid);
      }
    } else Serial.println("[LEARN FAILED] NVS khong ghi duoc; chua luu UID.");
    learningNode[0]=0;
  }
  for (size_t i=0;i<MAP_NODE_COUNT;++i) if (!strcmp(tagUids[i],uid)) {
    observeNode(MAP_NODES[i],"RFID");return;
  }
  invalidatePosition("The chua duoc learn");
}
void pollRfid() {
  if (!readerAvailable || (uint32_t)(millis()-lastRfidPollMs)<20) return;
  lastRfidPollMs=millis();
  bool present=reader.PICC_IsNewCardPresent();
  // A card left at the charging station is HALTed after its first read.
  // WUPA obtains a fresh UID without requiring the robot/tag to move after reconnect.
  if (!present && rfidRefreshRequested && (uint32_t)(millis()-lastRfidWakeMs)>=250) {
    lastRfidWakeMs=millis();byte atqa[2]={};byte count=sizeof(atqa);
    present=reader.PICC_WakeupA(atqa,&count)==MFRC522::STATUS_OK;
  }
  if (!present || !reader.PICC_ReadCardSerial()) return;
  char uid[UID_BYTES]={};
  if (reader.uid.size==4 || reader.uid.size==7 || reader.uid.size==10) {
    for (byte i=0;i<reader.uid.size;++i) snprintf(uid+i*2,3,"%02X",reader.uid.uidByte[i]);
    onUidRead(uid);
  } else invalidatePosition("Do dai UID khong ho tro");
  reader.PICC_HaltA();reader.PCD_StopCrypto1();
  rfidRefreshRequested=false; // Unknown stationary cards do not flood UNKNOWN at poll rate.
}

int zoneIndex(const char* zone) {
  if (zone) for (int i=0;i<6;++i) if (!strcmp(ZONES[i],zone)) return i;
  return -1;
}

bool sameCommand(const ReceivedCommand& a,const ReceivedCommand& b) {
  if (a.kind != b.kind || a.pathCount != b.pathCount || strcmp(a.target,b.target)) return false;
  for (size_t i=0;i<a.pathCount;++i) if (strcmp(a.path[i],b.path[i])) return false;
  return true;
}

bool parseCommand(const char* payload,size_t length,ReceivedCommand& result) {
  if (!length || length>MAX_COMMAND_BYTES || memchr(payload,0,length)) return false;
  JsonDocument document;
  if (deserializeJson(document,payload,length,DeserializationOption::NestingLimit(4))) return false;
  if (!document.is<JsonObject>() || !document["cmd_id"].is<int32_t>() ||
      !document["cmd"].is<const char*>()) return false;
  result = ReceivedCommand{};
  result.id = document["cmd_id"].as<int32_t>();
  if (result.id<=0) return false;
  const char* name = document["cmd"].as<const char*>();
  if (!strcmp(name,"STOP")) result.kind = STOP_COMMAND;
  else if (!strcmp(name,"WAIT")) result.kind = WAIT_COMMAND;
  else if (!strcmp(name,"CHARGE_START")) result.kind = CHARGE_COMMAND;
  else if (!strcmp(name,"GOTO")) {
    result.kind = GOTO_COMMAND;
    if (!document["target"].is<const char*>() || !document["path"].is<JsonArray>()) return false;
    const char* target = document["target"].as<const char*>();
    if (!knownNode(target)) return false;
    strcpy(result.target,target);
    JsonArray route = document["path"].as<JsonArray>();
    if (route.size()>MAX_PATH_NODES) return false;
    for (JsonVariant node : route) {
      if (!node.is<const char*>() || !knownNode(node.as<const char*>())) return false;
      strcpy(result.path[result.pathCount++],node.as<const char*>());
    }
    if (result.pathCount && strcmp(result.path[result.pathCount-1],result.target)) return false;
  } else return false;
  return true;
}

bool publishText(const char* topic,const char* payload) {
  if (!mqtt.connected() || !mqtt.publish(topic,payload,false)) {
    Serial.printf("[TX FAILED] %s\n",topic);return false;
  }
  return true;
}

void sendAck(int32_t id) {
  if (!ackEnabled) {Serial.printf("[ACK OFF] Bo ACK #%ld de test retry\n",(long)id);return;}
  char text[16];snprintf(text,sizeof(text),"%ld",(long)id);
  if (publishText(ackTopic,text)) {
    Serial.printf("[ACK] %s -> %s (nhan lenh, chua den dich)\n",ackTopic,text);
    if (pendingAckId==id) pendingAckId=0;
  } else pendingAckId=id;
}

void processCommand() {
  if (!commandPending) return;
  commandPending = false;
  ReceivedCommand candidate{};
  if (!parseCommand(receivedJson,receivedLength,candidate)) {
    Serial.println("[REJECT] JSON/cmd_id/path/node sai; KHONG ACK");return;
  }
  for (const ReceivedCommand& old : history) if (old.id==candidate.id) {
    if (!sameCommand(old,candidate)) {Serial.println("[REJECT] Cung cmd_id nhung noi dung khac");return;}
    ++duplicateCommands;
    Serial.printf("[DUPLICATE] #%ld; ACK lai, khong nhan lai hanh dong\n",(long)candidate.id);
    sendAck(candidate.id);return;
  }
  if (candidate.id<=highestCommandId) {Serial.println("[REJECT] cmd_id cu/ngoai cache");return;}
  if (candidate.kind==GOTO_COMMAND && (!positionKnown ||
      (!candidate.pathCount && strcmp(currentNode,candidate.target)))) {
    Serial.println("[REJECT] GOTO can RFID hop le va path den dich; KHONG ACK");return;
  }
  if (candidate.kind==CHARGE_COMMAND && (!positionKnown ||
      (strcmp(currentNode,"S1") && strcmp(currentNode,"S2")) ||
      (*homeStation() && strcmp(currentNode,homeStation())))) {
    Serial.println("[REJECT] CHARGE_START can RFID o tram rieng; KHONG ACK");return;
  }
  // Test receiver nhan vao trang thai truoc khi ACK; khong chay motor trong callback.
  char savedId[16];snprintf(savedId,sizeof(savedId),"%ld",(long)candidate.id);
  if (!nvsReady || tagStorage.putString("last-cmd",savedId) != strlen(savedId)) {
    enterFailsafe("Khong luu duoc cmd_id vao NVS");return;
  }
  lastCommand=candidate;history[historyNext]=candidate;historyNext=(historyNext+1)%HISTORY_SIZE;
  highestCommandId=candidate.id;++acceptedCommands;
  routeCursor=0;segmentArmed=false;AgvDrive::stop();
  if (candidate.kind==GOTO_COMMAND) receiverMode=ROUTE_TEST;
  else if (candidate.kind==CHARGE_COMMAND) receiverMode=CHARGE_TEST;
  else if (candidate.kind==STOP_COMMAND) receiverMode=STOPPED_TEST;
  else receiverMode=positionKnown ? READY_TEST : AWAIT_RFID;
  motionAllowed = candidate.kind==GOTO_COMMAND;
  failsafeLatched = false;
  lastControllerSeenMs = millis();
  sendAck(candidate.id);
  const char* names[] = {"GOTO","STOP","WAIT","CHARGE_START"};
  Serial.printf("[RX] #%ld %s\n",(long)candidate.id,names[candidate.kind]);
  if (candidate.kind==GOTO_COMMAND) {
    Serial.printf("  target=%s | path:",candidate.target);
    for (size_t i=0;i<candidate.pathCount;++i) Serial.printf(" %s",candidate.path[i]);
    Serial.println();Serial.println("  Motor dung tai moi RFID; dung motor arm cho tung doan sau kiem tra duong/vung.");
  } else if (candidate.kind==CHARGE_COMMAND) {
    Serial.println("  Chi log lenh sac; go charged 90 neu muon gia lap sac xong.");
  }
}

void onMqttMessage(char* topic,byte* payload,unsigned int length) {
  // PubSubClient co the ghi de buffer khi publish. Copy du lieu, xu ly o loop().
  if (!strcmp(topic,commandTopic)) {
    if (!commandPending && length && length<=MAX_COMMAND_BYTES) {
      memcpy(receivedJson,payload,length);receivedJson[length]=0;
      receivedLength=length;commandPending=true;
    } else Serial.println("[REJECT] Command qua dai/rong hoac receiver ban");
  } else if (!strcmp(topic,grantTopic)) {
    if (length && length<sizeof(receivedGrant) && !memchr(payload,0,length)) {
      memcpy(receivedGrant,payload,length);receivedGrant[length]=0;grantPending=true;
    }
  } else if (!strcmp(topic,heartbeatTopic)) {
    if (!length || length >= sizeof(controllerSession) || memchr(payload,0,length)) return;
    char session[sizeof(controllerSession)] = {};memcpy(session,payload,length);
    if (*controllerSession && strcmp(session,controllerSession)) enterFailsafe("Core session thay doi");
    strcpy(controllerSession,session);lastControllerSeenMs = millis();
  }
}

void processGrant() {
  if (!grantPending) return;
  grantPending=false;
  JsonDocument document;
  if (deserializeJson(document,receivedGrant) || !document["zone"].is<const char*>() || !document["grant_id"].is<int32_t>()) return;
  const char* name = document["zone"].as<const char*>();
  const int32_t grantId = document["grant_id"].as<int32_t>();
  const int zone=zoneIndex(name);
  if (grantId <= 0 || (zone>=0 && grantId < grantIds[zone])) return;
  if (zone<0 || !(requestedZones & (1<<zone))) {
    Serial.println("[IGNORE] Grant khong khop zone da xin");return;
  }
  grantedZones |= (1<<zone);
  grantIds[zone] = grantId;lastControllerSeenMs = millis();
  if (ackEnabled) publishText(mutexAckTopic,receivedGrant);
  Serial.printf("[MUTEX GRANT] %s #%ld; ACK nhan quyen, chua roi vung\n",name,(long)grantId);
}
void retryMutexRequests() {
  if (!mqtt.connected() || !positionKnown || failsafeLatched || (uint32_t)(millis()-lastMutexRequestMs)<1000) return;
  lastMutexRequestMs = millis();
  for (size_t i=0;i<6;++i) if ((requestedZones & (1<<i)) && !(grantedZones & (1<<i))) publishText(requestTopic,ZONES[i]);
}

void sendTelemetry() {
  if (!telemetryEnabled || !mqtt.connected()) return;
  char text[32];snprintf(text,sizeof(text),"%s|%u|%u",positionKnown ? currentNode : "UNKNOWN",batteryPercent,
    unsigned(obstacleFlag || (TestConfig::ENABLE_AUTONOMOUS_MOTION && AgvDrive::obstacle())));
  if (publishText(telemetryTopic,text)) Serial.printf("[TELEMETRY %s] %s\n",positionKnown ? (simulationEnabled ? "simulated" : "RFID") : "await RFID",text);
  lastTelemetryMs=millis();
}

bool parsePercent(const char* value,long& result) {
  if (!value || !*value) return false;
  for (const char* p=value;*p;++p) if (*p<'0' || *p>'9') return false;
  char* end;result=strtol(value,&end,10);
  return !*end && result>=0 && result<=100;
}

void printHelp() {
  Serial.println("help | status | learn S1 (hoac bat ky node) | learn cancel | tags | forget S1");
  Serial.println("simulate on/off | node S1 (CHI KHI simulate on) | battery 85 | obstacle 0");
  Serial.println("request M5 | release M5 | charged 90 | error TEST_ERROR");
  Serial.println("ack off/on | telemetry off/on (chi de test loi mang)");
  Serial.println("motor arm/off: cho phep/dung tung doan RFID khi enable motion va GOTO hop le");
  Serial.println("Pin va sac van la du lieu test; motor mac dinh khoa cho den khi kiem thu ban.");
}

void requestZoneForNextSegment() {
  if (!segmentArmed || !motionAllowed || routeCursor>=lastCommand.pathCount || !mqtt.connected()) return;
  const int zones[] = {zoneIndex(currentNode),zoneIndex(lastCommand.path[routeCursor])};
  for (int zone : zones) if (zone>=0 && !(requestedZones & (1<<zone)) &&
      publishText(requestTopic,ZONES[zone])) {
      requestedZones |= 1<<zone;
      Serial.printf("[MUTEX REQUEST] %s; cho grant truoc khi di\n",ZONES[zone]);
  }
}

bool hasZoneGrantsForNextSegment() {
  if (routeCursor>=lastCommand.pathCount) return false;
  const int currentZone=zoneIndex(currentNode);
  const int nextZone=zoneIndex(lastCommand.path[routeCursor]);
  return (currentZone<0 || (grantedZones & (1<<currentZone))) &&
         (nextZone<0 || (grantedZones & (1<<nextZone)));
}

bool nextSegmentPermitted() {
  if (!TestConfig::ENABLE_AUTONOMOUS_MOTION || !segmentArmed || !motionAllowed ||
      failsafeLatched || simulationEnabled || !configured || !mqtt.connected() ||
      !positionKnown || receiverMode!=ROUTE_TEST || obstacleFlag ||
      !*controllerSession || (uint32_t)(millis()-lastControllerSeenMs)>CONTROLLER_TIMEOUT_MS ||
      routeCursor>=lastCommand.pathCount) return false;
  return hasZoneGrantsForNextSegment();
}

void handleSerialLine(char* line) {
  char* value=strchr(line,' ');if (value) {*value++=0;while (*value==' ') ++value;}
  long number=0;
  if (!strcmp(line,"help")) printHelp();
  else if (!strcmp(line,"status")) {
    Serial.printf("[STATUS] id=%s home=%s WiFi=%d MQTT=%d node=%s mode=%s simulate=%d battery(TEST)=%u obstacle(TEST)=%u\n",
      ROBOT_ID,homeStation(),WiFi.status()==WL_CONNECTED,mqtt.connected(),positionKnown ? currentNode : "UNKNOWN",
      modeName(),simulationEnabled,batteryPercent,obstacleFlag);
    Serial.printf("accepted=%lu duplicate=%lu last_cmd_id=%ld ACK=%d telemetry=%d held_zone_mask=%u\n",
      (unsigned long)acceptedCommands,(unsigned long)duplicateCommands,(long)highestCommandId,
      ackEnabled,telemetryEnabled,grantedZones);
    Serial.printf("motor_enabled=%d segment_armed=%d running=%d sonic_blocked=%d line_fault=%d\n",
      TestConfig::ENABLE_AUTONOMOUS_MOTION,segmentArmed,AgvDrive::motorRunning(),
      AgvDrive::obstacle(),AgvDrive::lineFault());
  } else if (!strcmp(line,"learn") && value && !strcmp(value,"cancel")) {learningNode[0]=0;Serial.println("[LEARN] Da huy cho the");}
  else if (!strcmp(line,"learn") && knownNode(value)) {
    if (!nvsReady || simulationEnabled) Serial.println("[LEARN REJECT] Can NVS hoat dong va simulate off");
    else {strcpy(learningNode,value);rfidRefreshRequested=true;Serial.printf("[LEARN] Cho the tai %s; co the quet lai the dang o dau doc\n",value);}
  } else if (!strcmp(line,"tags")) {
    for (size_t i=0;i<MAP_NODE_COUNT;++i) if (*tagUids[i]) Serial.printf("[TAG] %s -> %s\n",MAP_NODES[i],tagUids[i]);
  } else if (!strcmp(line,"forget") && knownNode(value)) {
    const int index=nodeIndex(value);
    char key[16];snprintf(key,sizeof(key),"uid-%s",value);
    if (nvsReady && tagStorage.remove(key)) {
      strcpy(tagUids[index],defaultUidForNode(value));
      Serial.printf("[FORGET] %s -> UID mac dinh %s\n",value,tagUids[index]);
      if (positionKnown && !strcmp(currentNode,value)) invalidatePosition("Mapping da doi; can quet lai");
    } else Serial.println("[FORGET FAILED] Node khong co override hoac NVS loi");
  } else if (!strcmp(line,"simulate") && value && (!strcmp(value,"on") || !strcmp(value,"off"))) {
    simulationEnabled=!strcmp(value,"on");learningNode[0]=0;
    invalidatePosition(simulationEnabled ? "Bat che do gia lap bang tay" : "Quay lai RC522, can quet the moi");
  } else if (!strcmp(line,"node") && knownNode(value)) {
    if (simulationEnabled) observeNode(value,"SIMULATED");
    else Serial.println("[REJECT] node chi dung khi simulate on; mac dinh can quet RC522");
  }
  else if (!strcmp(line,"motor") && value && !strcmp(value,"off")) {
    segmentArmed=false;AgvDrive::stop();Serial.println("[MOTOR] Da dung va huy arm doan hien tai");
  } else if (!strcmp(line,"motor") && value && !strcmp(value,"arm")) {
    if (!TestConfig::ENABLE_AUTONOMOUS_MOTION || !motionAllowed || !positionKnown ||
        failsafeLatched || simulationEnabled || receiverMode!=ROUTE_TEST ||
        routeCursor>=lastCommand.pathCount || !mqtt.connected() || !*controllerSession) {
      Serial.println("[MOTOR LOCKED] Can enable da kiem thu ban, GOTO, RFID, MQTT va heartbeat");
    } else {
      segmentArmed=true;requestZoneForNextSegment();
      Serial.printf("[MOTOR ARMED] Doan %s -> %s; cho sensor va grant neu can\n",
        currentNode,lastCommand.path[routeCursor]);
    }
  }
  else if (!strcmp(line,"battery") && parsePercent(value,number)) {batteryPercent=number;sendTelemetry();}
  else if (!strcmp(line,"obstacle") && parsePercent(value,number) && number<=1) {obstacleFlag=number;sendTelemetry();}
  else if (!strcmp(line,"charged") && parsePercent(value,number)) {
    if (positionKnown && receiverMode==CHARGE_TEST && lastCommand.id && lastCommand.kind==CHARGE_COMMAND &&
        (!strcmp(currentNode,"S1") || !strcmp(currentNode,"S2")) &&
        (!*homeStation() || !strcmp(currentNode,homeStation()))) {
      if (publishText(chargedTopic,value)) {batteryPercent=number;receiverMode=READY_TEST;sendTelemetry();}
    } else Serial.println("[REJECT] Can CHARGE_START moi va RFID tai tram rieng");
  } else if (!strcmp(line,"error") && value && *value) publishText(errorTopic,value);
  else if ((!strcmp(line,"request") || !strcmp(line,"release")) && zoneIndex(value)>=0) {
    const uint8_t mask=1<<zoneIndex(value);
    if (!strcmp(line,"request")) {
      if (!positionKnown) Serial.println("[REJECT] Can RFID hop le truoc khi xin zone");
      else if (publishText(requestTopic,value)) requestedZones |= mask;
    }
    else if (grantedZones & mask) {
      char release[96];snprintf(release,sizeof(release),"{\"zone\":\"%s\",\"grant_id\":%ld}",value,(long)grantIds[zoneIndex(value)]);
      if (publishText(releaseTopic,release)) {requestedZones &= ~mask;grantedZones &= ~mask;}
    } else Serial.println("[REJECT] Chua nhan grant cho zone nay");
  } else if (!strcmp(line,"ack") && value && (!strcmp(value,"on") || !strcmp(value,"off"))) {
    ackEnabled=!strcmp(value,"on");pendingAckId=0;
    Serial.printf("[TEST] ACK=%d\n",ackEnabled);
  } else if (!strcmp(line,"telemetry") && value && (!strcmp(value,"on") || !strcmp(value,"off"))) {
    telemetryEnabled=!strcmp(value,"on");sendTelemetry();
  } else Serial.println("[SERIAL] Lenh/ID/tham so sai. Go help.");
}

void readSerial() {
  // Gioi han moi vong de MQTT loop van duoc phuc vu khi Serial nhan nhieu byte.
  for (int count=0;count<64 && Serial.available();++count) {
    const char c=Serial.read();if (c=='\r') continue;
    if (c=='\n') {
      if (serialOverflow) Serial.println("[SERIAL] Dong qua dai, bo qua");
      else if (serialLength) {serialLine[serialLength]=0;handleSerialLine(serialLine);}
      serialLength=0;serialOverflow=false;
    } else if (!serialOverflow) {
      if (serialLength<sizeof(serialLine)-1) serialLine[serialLength++]=c;
      else serialOverflow=true;
    }
  }
}

void serviceConnections() {
  if (!configured) return;
  const uint32_t now=millis();
  if (WiFi.status()!=WL_CONNECTED) {
    if (wasConnected) {wasConnected=false;enterFailsafe("Mat WiFi, can RFID moi");}
    if (mqtt.connected()) mqtt.disconnect();
    if ((uint32_t)(now-lastWifiTryMs)>=WIFI_RETRY_MS) {lastWifiTryMs=now;WiFi.reconnect();}
    return;
  }
  if (mqtt.connected()) return;
  if (wasConnected) {wasConnected=false;enterFailsafe("Mat MQTT, can RFID moi");}
  if ((uint32_t)(now-lastMqttTryMs)<MQTT_RETRY_MS) return;
#if !defined(AGV_FIRMWARE_HOST)
  if (MQTT_USE_TLS && time(nullptr)<1700000000) return; // Cho NTP truoc khi xac minh cert.
#endif
  lastMqttTryMs=now;
  Serial.printf("[MQTT CONNECT] %s:%u | ESP IP=%s | gateway=%s | client=%s\n",
    MQTT_HOST,MQTT_PORT,WiFi.localIP().toString().c_str(),WiFi.gatewayIP().toString().c_str(),clientId);
  const bool connected = mqtt.connect(clientId,*MQTT_USER ? MQTT_USER : nullptr,*MQTT_USER ? MQTT_PASSWORD : nullptr,statusTopic,1,true,"offline");
  if (!connected) {Serial.printf("[MQTT FAILED] state=%d | ESP IP=%s | gateway=%s; kiem tra IP/listener/auth/firewall\n",
    mqtt.state(),WiFi.localIP().toString().c_str(),WiFi.gatewayIP().toString().c_str());return;}
  if (!mqtt.subscribe(commandTopic,0) || !mqtt.subscribe(grantTopic,0) || !mqtt.subscribe(heartbeatTopic,0)) {
    Serial.println("[SUBSCRIBE FAILED]");mqtt.disconnect();return;
  }
  wasConnected=true;
  mqtt.publish(statusTopic,"online",true);lastControllerSeenMs = millis();
  Serial.printf("[CONNECTED] WiFi IP=%s | sub %s va %s\n",WiFi.localIP().toString().c_str(),commandTopic,grantTopic);
  sendTelemetry();
}

void setup() {
  Serial.begin(115200);
  AgvDrive::begin();
  Serial.println("\nAGV RC522 + MQTT v0 + L298N interlock (motor off at boot)");
  // Topic co ba segment, ID robot khong duoc chua slash/pipe/space.
  bool validId=*ROBOT_ID && strlen(ROBOT_ID)<=16;
  for (const char* p=ROBOT_ID;*p;++p) if (!(isalnum((unsigned char)*p) || *p=='_' || *p=='-')) validId=false;
  if (!validId) {Serial.println("[CONFIG ERROR] ROBOT_ID sai");return;}
  snprintf(commandTopic,sizeof(commandTopic),"agv/%s/command",ROBOT_ID);
  snprintf(telemetryTopic,sizeof(telemetryTopic),"agv/%s/telemetry",ROBOT_ID);
  snprintf(ackTopic,sizeof(ackTopic),"agv/%s/ack",ROBOT_ID);
  snprintf(grantTopic,sizeof(grantTopic),"agv/%s/mutex_grant",ROBOT_ID);
  snprintf(requestTopic,sizeof(requestTopic),"agv/%s/mutex_request",ROBOT_ID);
  snprintf(releaseTopic,sizeof(releaseTopic),"agv/%s/mutex_release",ROBOT_ID);
  snprintf(errorTopic,sizeof(errorTopic),"agv/%s/error",ROBOT_ID);
  snprintf(chargedTopic,sizeof(chargedTopic),"agv/%s/charging_done",ROBOT_ID);
  snprintf(mutexAckTopic,sizeof(mutexAckTopic),"agv/%s/mutex_ack",ROBOT_ID);
  snprintf(statusTopic,sizeof(statusTopic),"agv/%s/status",ROBOT_ID);
  initRfid();
  if (nvsReady) {
    const auto saved = tagStorage.getString("last-cmd","");
    if (saved.length()) {
      char* end;const long id = strtol(saved.c_str(),&end,10);
      if (*end || id < 1 || id > INT32_MAX) {Serial.println("[NVS ERROR] cmd_id sai");return;}
      highestCommandId = (int32_t)id;
    }
  }
  if (MQTT_USE_TLS) {
    if (!*MQTT_CA_PEM || !*MQTT_USER || !*MQTT_PASSWORD) {Serial.println("[CONFIG ERROR] TLS can CA/user/password");return;}
    secureTransport.setCACert(MQTT_CA_PEM);mqtt.setClient(secureTransport);
#if !defined(AGV_FIRMWARE_HOST)
    configTime(0,0,"pool.ntp.org","time.google.com");
#endif
  }
  if (!mqtt.setBufferSize(MQTT_BUFFER_BYTES)) {Serial.println("[CONFIG ERROR] Khong du RAM MQTT buffer");return;}
  mqtt.setServer(MQTT_HOST,MQTT_PORT);mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(15);mqtt.setSocketTimeout(2);transport.setConnectionTimeout(1500);
  printHelp();
  if (!strcmp(WIFI_SSID,"YOUR_WIFI_SSID") || !strcmp(WIFI_PASSWORD,"YOUR_WIFI_PASSWORD")) {
    Serial.println("[CONFIG ERROR] Hay sua WIFI_SSID/WIFI_PASSWORD roi upload lai");return;
  }
  WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  const auto mac=ESP.getEfuseMac();
  snprintf(clientId,sizeof(clientId),"agv-test-%s-%04lx%08lx",ROBOT_ID,
    (unsigned long)((mac>>32)&0xffff),(unsigned long)(mac&0xffffffff));
  configured=true;
}

void loop() {
  serviceConnections();
  if (mqtt.connected()) mqtt.loop();
  if (motionAllowed && (uint32_t)(millis()-lastControllerSeenMs)>CONTROLLER_TIMEOUT_MS) enterFailsafe("Core heartbeat timeout");
  processCommand();processGrant();readSerial();pollRfid();
  requestZoneForNextSegment();
  retryMutexRequests();
  const bool wasMoving=AgvDrive::motorRunning();
  AgvDrive::tick(nextSegmentPermitted());
  if (wasMoving && !AgvDrive::motorRunning() && segmentArmed &&
      (AgvDrive::obstacle() || AgvDrive::lineFault())) {
    segmentArmed=false;
    if (AgvDrive::lineFault()) publishText(errorTopic,"LINE_LOST");
    sendTelemetry();
    Serial.println("[MOTOR STOP] Mat line hoac vat can; can kiem tra va arm lai");
  }
  if (pendingAckId && ackEnabled && mqtt.connected()) sendAck(pendingAckId);
  if ((uint32_t)(millis()-lastTelemetryMs)>=TELEMETRY_INTERVAL_MS) sendTelemetry();
}
