#include "MQTTModule.h"
#include "core/CoreProtocol.h"
#include "core/CommandReplayGuard.h"
#include "core/ControlLogic.h"
#include "NetworkHealth.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

namespace {
const char* resetReasonName() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WATCHDOG";
        case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
        case ESP_RST_WDT: return "WATCHDOG";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}
struct ZoneRelease { char zone[NODE_TEXT_SIZE]{}; int32_t grantId = 0; };
void number(JsonObject object, const char* key, float value) {
    if (std::isfinite(value)) object[key] = value;
    else object[key] = nullptr;
}
void base(JsonObject object, uint32_t session, uint32_t uptime) {
    object["version"] = 1;
    object["agvId"] = Config::AGV_ID;
    object["session"] = session;
    object["uptimeMs"] = uptime;
}
bool serialize(const JsonDocument& doc, char* output, size_t capacity) {
    if (doc.overflowed() || measureJson(doc) >= capacity) return false;
    return serializeJson(doc, output, capacity) > 0;
}
bool topic(char* output, size_t capacity, const char* id, const char* action) {
    const int size = std::snprintf(output, capacity, "agv/%s/%s", id, action);
    return size > 0 && static_cast<size_t>(size) < capacity;
}
}

bool MQTTModule::configured() const {
    const char* id = Config::AGV_ID;
    if (!id || std::strlen(id) != 2 ||
        (std::strcmp(id, "01") != 0 && std::strcmp(id, "02") != 0) ||
        !Config::WIFI_SSID[0] || !Config::MQTT_SERVER[0] || !Config::MQTT_PORT ||
        std::strcmp(Config::MQTT_SERVER, "127.0.0.1") == 0 ||
        std::strcmp(Config::MQTT_SERVER, "localhost") == 0 ||
        std::strcmp(Config::MQTT_SERVER, "0.0.0.0") == 0) return false;
    if (static_cast<bool>(Config::MQTT_USERNAME[0]) != static_cast<bool>(Config::MQTT_PASSWORD[0])) return false;
    return !Config::MQTT_USE_TLS ||
        (Config::MQTT_CA_PEM[0] &&
         Config::MQTT_USERNAME[0] && Config::MQTT_PASSWORD[0]);
}

bool MQTTModule::buildTopics() {
    if (!configured()) return false;
    return topic(commandTopic_, sizeof(commandTopic_), Config::AGV_ID, "command") &&
        topic(ackTopic_, sizeof(ackTopic_), Config::AGV_ID, "ack") &&
        topic(telemetryTopic_, sizeof(telemetryTopic_), Config::AGV_ID, "telemetry") &&
        topic(statusTopic_, sizeof(statusTopic_), Config::AGV_ID, "status") &&
        topic(errorTopic_, sizeof(errorTopic_), Config::AGV_ID, "error") &&
        topic(eventTopic_, sizeof(eventTopic_), Config::AGV_ID, "event") &&
        topic(diagnosticTopic_, sizeof(diagnosticTopic_), Config::AGV_ID, "diagnostic") &&
        topic(odometryTopic_, sizeof(odometryTopic_), Config::AGV_ID, "odometry") &&
        topic(grantTopic_, sizeof(grantTopic_), Config::AGV_ID, "mutex_grant") &&
        topic(requestTopic_, sizeof(requestTopic_), Config::AGV_ID, "mutex_request") &&
        topic(grantAckTopic_, sizeof(grantAckTopic_), Config::AGV_ID, "mutex_ack") &&
        topic(releaseTopic_, sizeof(releaseTopic_), Config::AGV_ID, "mutex_release");
}

bool MQTTModule::begin() {
    if (!buildTopics()) {
        if (Config::DEBUG_ENABLED) Serial.println("[MQTT] Invalid robot ID or network/broker/TLS configuration");
        return false;
    }
    if (!commandStore_.begin("agv-rfid", false)) return false;
    const String saved = commandStore_.getString("last-cmd", "");
    if (saved.length()) {
        char* end = nullptr;
        const long value = std::strtol(saved.c_str(), &end, 10);
        if (!end || *end || value < 1 || value > INT_MAX) return false;
        highwater_ = static_cast<int32_t>(value);
    }
    commands_ = xQueueCreate(Config::COMMAND_QUEUE_LENGTH, sizeof(CoreCommand));
    events_ = xQueueCreate(Config::EVENT_QUEUE_LENGTH, sizeof(Outbound));
    telemetry_ = xQueueCreate(1, sizeof(TelemetrySnapshot));
    if (Config::ENCODER_ENABLED) {
        odometry_ = xQueueCreate(1, sizeof(OdometrySnapshot));
        encoderBootId_ = esp_random();
        if (!encoderBootId_) encoderBootId_ = 1;
        if (!odometry_) return false;
    }
    grants_ = xQueueCreate(6, sizeof(CoreGrant));
    zoneRequests_ = xQueueCreate(1, NODE_TEXT_SIZE);
    zoneReleases_ = xQueueCreate(6, sizeof(ZoneRelease));
    if (!commands_ || !events_ || !telemetry_ || !grants_ || !zoneRequests_ || !zoneReleases_) return false;
    client_.setServer(Config::MQTT_SERVER, Config::MQTT_PORT);
    if (Config::MQTT_USE_TLS) {
        secureTransport_.setCACert(Config::MQTT_CA_PEM);
        client_.setClient(secureTransport_);
    }
    client_.setSocketTimeout(Config::MQTT_SOCKET_TIMEOUT_SECONDS);
    client_.setKeepAlive(Config::MQTT_KEEPALIVE_SECONDS);
    if (!client_.setBufferSize(Config::MAX_PUBLISH_BYTES + 256)) return false;
    transport_.setTimeout(Config::MQTT_SOCKET_TIMEOUT_SECONDS);
    secureTransport_.setTimeout(Config::MQTT_SOCKET_TIMEOUT_SECONDS);
    client_.setCallback([this](char* topicName, uint8_t* payload, unsigned int length) {
        onMessage(topicName, payload, length);
    });
    servicedAtMs_ = millis();
    const BaseType_t result = xTaskCreatePinnedToCore(workerEntry, "agv-network", Config::NETWORK_TASK_STACK_BYTES, this, 1, &worker_, 0);
    return result == pdPASS;
}

void MQTTModule::workerEntry(void* argument) {
    auto* self = static_cast<MQTTModule*>(argument);
    self->wifi_.begin();
    for (;;) { self->loop(); vTaskDelay(pdMS_TO_TICKS(10)); }
}

bool MQTTModule::isConnected() const {
    return NetworkHealth::connected([this] {
        return NetworkHealth::Snapshot{ready_.load(),wifi_.isConnected(),heartbeatSeen_.load(),coreRestart_.load(),
            servicedAtMs_.load(),lastHeartbeatMs_.load()};
    }, [] { return millis(); });
}

bool MQTTModule::subscribeTopics() {
    return client_.subscribe(commandTopic_, 0) && client_.subscribe(grantTopic_, 0) &&
           client_.subscribe(Config::MQTT_HEARTBEAT_TOPIC, 0);
}

void MQTTModule::connect() {
    lastAttemptMs_ = millis();
    if (!wifi_.isConnected() || !configured()) return;
    if (Config::MQTT_USE_TLS && std::time(nullptr) < 1700000000) {
        if (!timeSyncRequested_) {
            configTime(0, 0, Config::NTP_SERVER);
            timeSyncRequested_ = true;
        }
        return;
    }
    String mac = WiFi.macAddress(); mac.replace(":", "");
    if (mac.length() != 12) return;
    char clientId[40];
    const int size = std::snprintf(clientId, sizeof(clientId), "agv-%s-%s", Config::AGV_ID, mac.c_str());
    if (size < 1 || static_cast<size_t>(size) >= sizeof(clientId)) return;
    const char* user = Config::MQTT_USERNAME[0] ? Config::MQTT_USERNAME : nullptr;
    const char* password = user ? Config::MQTT_PASSWORD : nullptr;
    if (!client_.connect(clientId, user, password, statusTopic_, 1, true, "offline") || !subscribeTopics()) {
        if (Config::DEBUG_ENABLED) Serial.printf("[MQTT] Connect failed state=%d\n", client_.state());
        transport_.stop(); secureTransport_.stop();
        ready_ = false; stop_ = true; return;
    }
    if (!client_.publish(statusTopic_, "online", true)) {
        transport_.stop(); secureTransport_.stop();
        ready_ = false; stop_ = true; return;
    }
    xQueueReset(telemetry_); // Do not replay a position measured before reconnect.
    if (odometry_) xQueueReset(odometry_);
    xQueueReset(grants_);
    xQueueReset(zoneRequests_);
    xQueueReset(zoneReleases_);
    pendingZone_[0] = '\0';
    uint32_t generation = session_.fetch_add(1) + 1;
    if (!generation) session_ = 1;
    heartbeatSeen_ = false;
    stop_ = true;
    ready_ = true;
    if (Config::DEBUG_ENABLED) Serial.printf("[MQTT] Connected %s; awaiting Core heartbeat\n", clientId);
}

void MQTTModule::loop() {
    wifi_.loop();
    if (!wifi_.isConnected()) {
        if (ready_.exchange(false)) stop_ = true;
        heartbeatSeen_ = false;
        transport_.stop();
        secureTransport_.stop();
    }
    if (wifi_.isConnected() && !client_.connected() &&
        static_cast<uint32_t>(millis() - lastAttemptMs_) >= Config::MQTT_RETRY_MS) connect();
    if (client_.connected()) {
        if (!client_.loop()) {
            ready_ = false; stop_ = true; heartbeatSeen_ = false;
            transport_.stop(); secureTransport_.stop();
        }
        if (heartbeatSeen_.load() &&
            static_cast<uint32_t>(millis() - lastHeartbeatMs_.load()) >= Config::CORE_HEARTBEAT_TIMEOUT_MS) {
            heartbeatSeen_ = false; stop_ = true; coreRestart_ = true;
            xQueueReset(telemetry_);
            if (odometry_) xQueueReset(odometry_);
            uint32_t generation = session_.fetch_add(1) + 1;
            if (!generation) session_ = 1;
        }
        if (ready_.load() && heartbeatSeen_.load() && !coreRestart_.load()) {
            if (xQueueReceive(zoneRequests_, pendingZone_, 0) == pdTRUE &&
                !client_.publish(requestTopic_, pendingZone_, false)) {
                ready_ = false; stop_ = true;
                transport_.stop(); secureTransport_.stop();
            }
            if (pendingAckId_ > 0) sendAck(pendingAckId_);
            ZoneRelease release;
            if (xQueuePeek(zoneReleases_, &release, 0) == pdTRUE) {
                char payload[96];
                const int length = std::snprintf(payload, sizeof(payload),
                    "{\"zone\":\"%s\",\"grant_id\":%ld}", release.zone,
                    static_cast<long>(release.grantId));
                if (length > 0 && static_cast<size_t>(length) < sizeof(payload) &&
                    client_.publish(releaseTopic_, payload, false))
                    xQueueReceive(zoneReleases_, &release, 0);
            }
            if (xQueuePeek(events_, &pending_, 0) == pdTRUE &&
                client_.publish(pending_.errorTopic ? errorTopic_ : eventTopic_, pending_.payload, false))
                xQueueReceive(events_, &pending_, 0);
            TelemetrySnapshot snapshot;
            if (xQueueReceive(telemetry_, &snapshot, 0) == pdTRUE) sendTelemetry(snapshot);
            OdometrySnapshot odometry;
            if (odometry_ && xQueueReceive(odometry_, &odometry, 0) == pdTRUE) sendOdometry(odometry);
        }
    } else {
        if (ready_.exchange(false)) stop_ = true;
        heartbeatSeen_ = false;
    }
    servicedAtMs_ = millis();
}

bool MQTTModule::popCommand(CoreCommand& command) {
    return commands_ && xQueueReceive(commands_, &command, 0) == pdTRUE;
}
bool MQTTModule::popGrant(CoreGrant& grant) {
    return grants_ && xQueueReceive(grants_, &grant, 0) == pdTRUE;
}
bool MQTTModule::requestZone(const char* zone) {
    if (!zone || zone[0] != 'M' || zone[1] < '1' || zone[1] > '6' || zone[2] ||
        !zoneRequests_ || !isConnected()) return false;
    char value[NODE_TEXT_SIZE]{};
    std::strcpy(value, zone);
    return xQueueOverwrite(zoneRequests_, value) == pdTRUE;
}
bool MQTTModule::releaseZone(const char* zone, int32_t grantId) {
    if (!zone || zone[0] != 'M' || zone[1] < '1' || zone[1] > '6' || zone[2] ||
        grantId < 1 || !zoneReleases_ || !isConnected()) return false;
    ZoneRelease release;
    std::strcpy(release.zone, zone);
    release.grantId = grantId;
    return xQueueSend(zoneReleases_, &release, 0) == pdTRUE;
}

bool MQTTModule::persistCommandId(int32_t cmdId) {
    if (cmdId <= highwater_) return false;
    char text[16];
    if (!CoreProtocol::ack(cmdId, text, sizeof(text))) return false;
    if (commandStore_.putString("last-cmd", text) != std::strlen(text)) return false;
    highwater_ = cmdId;
    return true;
}

bool MQTTModule::sendAck(int32_t cmdId) {
    char text[16];
    if (!CoreProtocol::ack(cmdId, text, sizeof(text))) return false;
    if (!client_.publish(ackTopic_, text, false)) {
        if (cmdId > pendingAckId_) pendingAckId_ = cmdId;
        return false;
    }
    if (pendingAckId_ > 0 && pendingAckId_ <= cmdId) pendingAckId_ = 0;
    return true;
}

void MQTTModule::onMessage(char* topicName, uint8_t* payload, unsigned int length) {
    if (std::strcmp(topicName, Config::MQTT_HEARTBEAT_TOPIC) == 0) {
        if (!payload || !length || length >= sizeof(coreSession_)) return;
        for (unsigned int i = 0; i < length; ++i) if (payload[i] == 0) return;
        const bool changed = coreSession_[0] &&
            (std::strlen(coreSession_) != length || std::memcmp(coreSession_, payload, length) != 0);
        std::memcpy(coreSession_, payload, length); coreSession_[length] = '\0';
        if (changed) {
            coreRestart_ = true; stop_ = true;
            xQueueReset(commands_);
            xQueueReset(telemetry_);
            if (odometry_) xQueueReset(odometry_);
            xQueueReset(grants_);
            xQueueReset(zoneRequests_);
            xQueueReset(zoneReleases_);
            for (auto& item : recentCommands_) item = CoreCommand{};
            nextId_ = 0;
            pendingAckId_ = 0;
            uint32_t generation = session_.fetch_add(1) + 1;
            if (!generation) session_ = 1;
        }
        lastHeartbeatMs_ = millis(); heartbeatSeen_ = true;
        return;
    }
    if (std::strcmp(topicName, grantTopic_) == 0) {
        if (!isConnected() || !payload || !length || length > 128) return;
        CoreGrant grant;
        if (!CoreProtocol::decodeGrant(payload,length,grant)) return;
        grant.localSession = session_.load();
        // publish() reuses the RX buffer. All fields must already be owned
        // by grant; never use a zero-copy JSON pointer after sending ACK.
        char ack[96];
        if (!CoreProtocol::grantAck(grant,ack,sizeof(ack)) ||
            !client_.publish(grantAckTopic_, ack, false)) return;
        if (xQueueSend(grants_, &grant, 0) != pdTRUE) queueFault_ = true;
        return;
    }
    if (std::strcmp(topicName, commandTopic_) != 0) return;
    CoreCommand command;
    if (!CoreProtocol::decodeCommand(payload, length, command)) {
        publishError(ErrorCode::InvalidCommand, millis()); return;
    }
    if (command.type == CoreCommandType::Stop || command.type == CoreCommandType::Wait) stop_ = true;
    if (command.type != CoreCommandType::Stop && command.type != CoreCommandType::Wait && !isConnected()) {
        publishError(ErrorCode::NotConnected, millis()); return;
    }
    const auto replay = CommandReplayGuard::classify(command, highwater_, recentCommands_);
    if (replay != CommandReplayGuard::Result::Fresh) {
        if (replay == CommandReplayGuard::Result::Duplicate) sendAck(command.cmdId);
        else publishError(replay == CommandReplayGuard::Result::Conflict
            ? ErrorCode::InvalidCommand : ErrorCode::StaleCommand, millis());
        return;
    }
    command.localSession = session_.load(); command.receivedAtMs = millis();
    if (!persistCommandId(command.cmdId)) { storageFault_ = true; publishError(ErrorCode::Configuration, millis()); return; }
    if (xQueueSend(commands_, &command, 0) != pdTRUE) { queueFault_ = true; return; }
    recentCommands_[nextId_] = command;
    nextId_ = (nextId_ + 1) % Config::RECENT_COMMAND_IDS;
    sendAck(command.cmdId); // Receipt only; RFID telemetry proves arrival.
}
bool MQTTModule::enqueue(const Outbound& message) {
    if (!events_ || !worker_) return false;
    if (xQueueSend(events_, &message, 0) != pdTRUE) { queueFault_ = true; return false; }
    return true;
}
void MQTTModule::publishStatus(const TelemetrySnapshot& snapshot) {
    if (telemetry_ && worker_ && xQueueOverwrite(telemetry_, &snapshot) != pdTRUE) queueFault_ = true;
}
bool MQTTModule::publishEvent(const char* name, const char* node, uint32_t uptime,
                              const char* taskId, const char* fromNode, bool reverse) {
    StaticJsonDocument<512> doc;
    auto object = doc.to<JsonObject>();
    base(object, session(), uptime);
    object["event"] = name;
    object["node"] = node;
    object["taskId"] = taskId;
    object["fromNode"] = fromNode;
    object["reverse"] = reverse;
    Outbound message;
    if (!serialize(doc, message.payload, sizeof(message.payload))) { queueFault_ = true; return false; }
    return enqueue(message);
}
bool MQTTModule::publishCargo(const char* name, const char* node, const char* taskId, bool present, uint32_t uptime) {
    StaticJsonDocument<384> doc;
    auto object = doc.to<JsonObject>();
    base(object, session(), uptime);
    object["event"] = name;
    object["node"] = node;
    object["taskId"] = taskId;
    object["cargoPresent"] = present;
    Outbound message;
    if (!serialize(doc, message.payload, sizeof(message.payload))) { queueFault_ = true; return false; }
    return enqueue(message);
}
bool MQTTModule::publishError(ErrorCode reason, uint32_t uptime) {
    (void)uptime;
    Outbound message;
    message.errorTopic = true;
    const char* name = errorName(reason);
    if (std::strlen(name) >= sizeof(message.payload)) { queueFault_ = true; return false; }
    std::strcpy(message.payload, name);
    return enqueue(message);
}

void MQTTModule::sendTelemetry(const TelemetrySnapshot& s) {
    if (s.localSession != session_.load()) return;
    const char* node = s.position.node[0] ? s.position.node : "UNKNOWN";
    // Keep the control telemetry unmeasured while battery display is being tested.
    // The percentage stays on diagnostic until battery thresholds are enabled.
    char wire[64];
    // Xe 01 báo vật cản theo diagnostic có hướng/khoảng cách. Cờ pipe cũ
    // khóa node kế và lập lại tuyến, trái với yêu cầu dừng/chạy tiếp tại cạnh.
    if (!CoreProtocol::telemetry(node, -1, Config::REAR_ULTRASONIC_ECHO_PIN < 0 && s.obstacle, wire, sizeof(wire)) ||
        !client_.publish(telemetryTopic_, wire, false)) {
        ready_ = false; stop_ = true; heartbeatSeen_ = false;
        transport_.stop(); secureTransport_.stop();
        return;
    }
    // RFID telemetry proves location; station diagnostics additionally gate
    // completion until the physical exit-facing pivot is finished.
    StaticJsonDocument<2048> doc;
    auto object = doc.to<JsonObject>();
    base(object, session(), s.uptimeMs);
    object["state"] = stateName(s.state);
    object["resetReason"] = resetReasonName();
    object["resetReasonCode"] = static_cast<int>(esp_reset_reason());
    object["firmwareMode"] = Config::TWO_ROBOT_BENCH_TRIAL ? "bench" : Config::ONE_ROBOT_TRIAL ? "solo" : "locked";
    object["firmwareBuild"] = Config::FIRMWARE_BUILD;
    // Worker-owned value persisted in NVS; Core advances its allocator above it.
    object["lastCommandId"] = highwater_;
    if (Config::ENCODER_ENABLED) {
        object["encoderSupported"] = true;
        object["encoderBootId"] = encoderBootId_;
        object["encoderCoreSessionId"] = coreSession_;
    }
    object["motionEnabled"] = Config::ENABLE_CORE_MOTION;
    object["headingSupported"] = true;
    object["stationDockingSupported"] = true;
    object["reverseHomeSupported"] = Config::REVERSE_HOME_SUPPORTED;
    if(Config::SERVO_SINGLE_TEST) {
        object["liftSupported"]=s.liftSupported;object["liftBusy"]=s.liftBusy;
        object["liftCommandId"]=s.liftCmdId;object["liftState"]=s.liftState;
        object["liftElapsedMs"]=s.liftElapsedMs;
        if(s.virtualCargoKnown) object["virtualCargo"]=s.virtualCargo;
        else object["virtualCargo"]=nullptr;
    }
    object["stationReady"] = s.stationReady;
    object["nodeOrdersSupported"] = true;
    object["straightThroughSupported"] = true;
    object["awaitingNodeOrder"] = s.awaitingNodeOrder;
    object["stationTurnRequired"] = s.stationTurnRequired;
    object["routeCommandId"] = s.routeCmdId;
    object["heading"] = headingName(s.heading);
    object["headingNode"] = node;
    object["headingSource"] = s.heading == RobotHeading::Unknown ? "unknown" : s.headingBayDefault ? "bay_default" : "route";
    object["error"] = errorName(s.error);
    object["node"] = node;
    if (s.rfidAgeMs != std::numeric_limits<uint32_t>::max()) object["rfidAgeMs"] = s.rfidAgeMs;
    else object["rfidAgeMs"] = nullptr;
    object["nextNode"] = s.nextNode;
    object["taskId"] = s.taskId;
    object["taskActive"] = s.taskActive;
    object["pathIndex"] = s.pathIndex;
    object["pathLength"] = s.pathLength;
    object["motor"] = motorActionName(s.motor);
    object["linePattern"] = s.linePattern;
    object["leftDuty"] = s.leftDuty;
    object["rightDuty"] = s.rightDuty;
    if (Config::ENCODER_ENABLED) {
        object["buzzerOn"] = s.buzzerOn; // Yêu cầu theo motor; không khẳng định đã phát âm.
        object["buzzerPinLevel"] = s.buzzerPinLevel; // GPIO0 đọc tại lúc chụp snapshot.
    }
    object["wifi"] = s.wifi;
    object["mqtt"] = s.mqtt;
    if (Config::ENCODER_ENABLED) {
        const uint32_t now = millis(); // Worker owns these timestamps; callback cannot change them here.
        object["networkServiceAgeMs"] = static_cast<uint32_t>(now-servicedAtMs_.load());
        if (heartbeatSeen_.load()) object["coreHeartbeatAgeMs"] = static_cast<uint32_t>(now-lastHeartbeatMs_.load());
        else object["coreHeartbeatAgeMs"] = nullptr;
        object["mqttClientState"] = client_.state();
        object["wifiRssi"] = WiFi.RSSI();
    }
    object["obstacle"] = s.obstacle;
    number(object, "ultrasonicCm", s.ultrasonicCm);
    if (Config::REAR_ULTRASONIC_ECHO_PIN >= 0) {
        object["obstacleDirection"] = s.obstacleDirection;
        number(object, "frontUltrasonicCm", s.frontUltrasonicCm);
        number(object, "rearUltrasonicCm", s.rearUltrasonicCm);
    }
    object["batteryAdcMv"] = s.batteryAdcMv;
    object["batteryCalibrated"] = s.batteryCalibrated;
    number(object, "batteryVoltage", s.batteryVoltage);
    number(object, "batteryPercentage", s.batteryPercentage);
    object["cargoStable"] = s.cargoStable;
    object["cargoPresent"] = s.cargoStable ? s.cargoPresent : false;
    char payload[Config::MAX_PUBLISH_BYTES];
    if (serialize(doc, payload, sizeof(payload))) client_.publish(diagnosticTopic_, payload, false);
}

void MQTTModule::publishOdometry(const OdometrySnapshot& snapshot) {
    if (Config::ENCODER_ENABLED && odometry_ && worker_) xQueueOverwrite(odometry_, &snapshot);
}
void MQTTModule::sendOdometry(const OdometrySnapshot& s) {
    if (!Config::ENCODER_ENABLED || s.localSession != session_.load() ||
        static_cast<uint32_t>(millis() - s.uptimeMs) > 500) return;
    char payload[Config::MAX_PUBLISH_BYTES];
    // Luồng hiển thị không phát command, không thay chốt motor hoặc mutex.
    if (CoreProtocol::odometry(s,encoderBootId_,coreSession_,payload,sizeof(payload)))
        client_.publish(odometryTopic_, payload, false);
}
