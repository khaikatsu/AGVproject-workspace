#pragma once
#include <string>
#include <vector>
#include <functional>
struct HostPublish {std::string topic,payload;bool retained;};
inline std::vector<HostPublish> hostPublishes;
inline std::function<void(const HostPublish&)> hostPublishHook;
class PubSubClient {
public:
    bool connectedFlag=true,publishSucceeds=true;
    explicit PubSubClient(WiFiClient&) {}
    bool connected() {return connectedFlag;}
    bool publish(const char* topic,const char* payload,bool retained) {
        if (!connectedFlag || !publishSucceeds) return false;
        HostPublish event{topic,payload,retained};hostPublishes.push_back(event);
        if (hostPublishHook) hostPublishHook(event);
        return true;
    }
    bool subscribe(const char*,uint8_t) {return true;}
    bool connect(const char*,const char* = nullptr,const char* = nullptr,const char* = nullptr,int = 0,bool = false,const char* = nullptr) {connectedFlag=true;return true;}
    void setClient(WiFiClient&) {}
    void disconnect() {connectedFlag=false;}
    bool setBufferSize(uint16_t) {return true;}
    void setServer(const char*,uint16_t) {}
    void setCallback(void (*)(char*,byte*,unsigned int)) {}
    void setKeepAlive(uint16_t) {}
    void setSocketTimeout(uint16_t) {}
    int state() {return connectedFlag ? 0 : -1;}
    void loop() {}
};
