#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <atomic>
#include "WiFiModule.h"
#include "Types.h"

class MQTTModule {
public:
    explicit MQTTModule(WiFiModule& wifi) : wifi_(wifi), client_(transport_) {}
    bool begin();
    void connect();
    void loop();
    bool subscribeTopics();
    bool isConnected() const;
    uint32_t session() const { return session_.load(); }
    bool popCommand(CoreCommand& command);
    bool popGrant(CoreGrant& grant);
    bool requestZone(const char* zone);
    bool releaseZone(const char* zone, int32_t grantId);
    bool consumeStop() { return stop_.exchange(false); }
    bool consumeQueueFault() { return queueFault_.exchange(false); }
    bool consumeStorageFault() { return storageFault_.exchange(false); }
    bool consumeCoreRestart() { return coreRestart_.exchange(false); }
    void publishStatus(const TelemetrySnapshot& snapshot);
    void publishPosition(const TelemetrySnapshot& snapshot) { publishStatus(snapshot); }
    void publishDistance(const TelemetrySnapshot& snapshot) { publishStatus(snapshot); }
    bool publishEvent(const char* name, const char* node, uint32_t uptimeMs,
                      const char* taskId = "", const char* fromNode = "", bool reverse = false);
    bool publishCargo(const char* name, const char* node, const char* taskId, bool present, uint32_t uptimeMs);
    bool publishError(ErrorCode reason, uint32_t uptimeMs);
private:
    struct Outbound { bool errorTopic = false; char payload[Config::MAX_PUBLISH_BYTES]{}; };
    static void workerEntry(void* argument);
    void onMessage(char* topic, uint8_t* payload, unsigned int length);
    bool enqueue(const Outbound& message);
    void sendTelemetry(const TelemetrySnapshot& snapshot);
    bool configured() const;
    bool buildTopics();
    bool persistCommandId(int32_t cmdId);
    bool sendAck(int32_t cmdId);
    WiFiModule& wifi_;
    WiFiClient transport_;
    WiFiClientSecure secureTransport_;
    PubSubClient client_;
    Preferences commandStore_;
    QueueHandle_t commands_ = nullptr, events_ = nullptr, telemetry_ = nullptr;
    QueueHandle_t grants_ = nullptr, zoneRequests_ = nullptr, zoneReleases_ = nullptr;
    TaskHandle_t worker_ = nullptr;
    std::atomic<bool> ready_{false}, stop_{false}, queueFault_{false}, storageFault_{false}, coreRestart_{false}, heartbeatSeen_{false};
    std::atomic<uint32_t> servicedAtMs_{0}, session_{0}, lastHeartbeatMs_{0};
    uint32_t lastAttemptMs_ = 0;
    CoreCommand recentCommands_[Config::RECENT_COMMAND_IDS]{};
    int32_t highwater_ = 0;
    int32_t pendingAckId_ = 0;
    size_t nextId_ = 0;
    char coreSession_[96]{};
    char commandTopic_[64]{}, ackTopic_[64]{}, telemetryTopic_[64]{}, statusTopic_[64]{};
    char errorTopic_[64]{}, eventTopic_[64]{}, diagnosticTopic_[64]{};
    char grantTopic_[64]{}, requestTopic_[64]{}, grantAckTopic_[64]{}, releaseTopic_[64]{};
    char pendingZone_[NODE_TEXT_SIZE]{};
    Outbound pending_{};
    bool timeSyncRequested_ = false;
};
