// Core AGV: MQTT chuỗi telemetry / JSON command, WS có request/result phiên bản 1.
// Core sở hữu graph/fleet/mutex; gateway sở hữu lịch sử/snapshot trong SQLite.
// Xem docs/14-web-server-update.md và contracts/web-server.md trước khi đổi hợp đồng.
// Build Windows: scripts/build-core.ps1. Entry point duy nhất: tệp này.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <chrono>
#include <deque>
#include <ctime>
#include <cstdint>
#include <csignal>
#include <filesystem>
#include <cmath>

// ---------------------------------------------------------------------------
// LOP SOCKET CHEO NEN TANG: thay vi chi dung winsock2.h (chi co tren Windows),
// Linux/Raspberry Pi dung thang BSD sockets (POSIX) co san trong glibc, KHONG
// can cai them thu vien socket nao. Dinh nghia lai vai ten (SOCKET,
// INVALID_SOCKET, SOCKET_ERROR, closesocket) de toan bo code WebSocket ben duoi
// (da viet san) dung chung duoc tren ca 2 nen tang ma khong phai sua gi them.
// ---------------------------------------------------------------------------
#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
  #pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
  using SOCKET = int;
  static constexpr SOCKET INVALID_SOCKET = -1;
  static constexpr int SOCKET_ERROR = -1;
  inline int closesocket(SOCKET s) { return ::close(s); }
#endif

// OpenSSL: dung chung cho SHA1 + Base64 cua bat tay WebSocket tren CA 2 nen tang
// (xem base64Encode()/sha1Base64() ben duoi), thay the BCrypt/CryptoAPI cu.
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <mosquitto.h>

#include "utils/json.hpp"
#include "map/MapGraph.hpp"
#include "utils/Mutexarbiter.hpp"
#include "core/FLeetmanager.hpp"
#include "commands/WebCommands.hpp"
#include "commands/CommandIds.hpp"
#include "core/RuntimeStore.hpp"
#include <random>
#include <cstdlib>

using json = nlohmann::json;

// Co Ctrl+C (SIGINT) de thoat chuong trinh sach se tren MOI nen tang, thay the
// GetAsyncKeyState(VK_ESCAPE) (chi hoat dong tren Windows, va chi khi cua so
// console dang duoc focus). Xem vong lap chinh trong main().
static std::atomic<bool> g_running{true};
static void onShutdownSignal(int) { g_running = false; }

// ---------------------------------------------------------------------------
// CO CHE HUONG SU KIEN (EVENT-DRIVEN) cho vong lap chinh: thay vi luon ngu du
// 700ms roi moi tick()/broadcast() (gay tre pha xu ly khi ESP32 vua bao vat
// can/loi qua MQTT), luong MQTT (on_message, chay tren luong rieng cua
// mosquitto_loop_start) goi requestWake() de DANH THUC NGAY vong lap chinh
// trong main() khi co su kien khan cap (obstacle/error) -- xem cac cho goi
// requestWake() trong on_message() ben duoi. 700ms van la "tran tren" (upper
// bound) cho cac lan tick() dinh ky binh thuong khi khong co su kien nao.
// ---------------------------------------------------------------------------
static std::mutex              g_wakeMtx;
static std::condition_variable g_wakeCv;
static std::atomic<bool>       g_wakeRequested{false};

static void requestWake() {
    { std::lock_guard<std::mutex> lk(g_wakeMtx); g_wakeRequested = true; }
    g_wakeCv.notify_one();
}
using namespace std;

// =========================================================================
// PHẦN A — TRẠNG THÁI TOÀN CỤC (glue giữa 3 module)
// =========================================================================
static MapGraph      g_map;
static MutexArbiter  g_arbiter;
static FleetManager  g_fleet(g_map, g_arbiter);
static mosquitto*    g_mosq = nullptr;
static std::atomic<bool> g_mqttConnected{false};
// Cổng 3000 dành cho HTTP/WebSocket gateway của Node.js.  C++ Core phải
// dùng cổng riêng để Node.js có thể kết nối đến nó mà không tranh chấp port.
static int envPort(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value) return fallback;
    int port = std::stoi(value);
    if (port < 1 || port > 65535) throw std::invalid_argument("Port khong hop le");
    return port;
}
static const int WS_PORT = envPort("AGV_CORE_WS_PORT", 8080);
static const string MAP_PATH = std::getenv("AGV_MAP_PATH") ? std::getenv("AGV_MAP_PATH") : "data/map_layout.json";
static WebCommands g_commands(g_map, g_fleet, g_arbiter, MAP_PATH);
static std::unique_ptr<ShiftController> g_shift;
static std::recursive_mutex g_stateMtx;
static std::unique_ptr<RuntimeStore> g_runtime;
static vector<pair<string,string>> g_outbound;
static bool g_storageHealthy = true;
static std::uint64_t g_snapshotSequence = 0;
static const string CORE_SESSION = to_string(chrono::system_clock::now().time_since_epoch().count()) + "-" + to_string(std::random_device{}());
static json stateSnapshot() {
    lock_guard<recursive_mutex> lk(g_stateMtx);
    json state = {{"protocol_version", 1}, {"core_session_id", CORE_SESSION},
            {"core_snapshot_seq",++g_snapshotSequence},
            {"mqtt_connected", g_mqttConnected.load()}, {"storage_healthy",g_storageHealthy}, {"map", g_map.toJson()}, {"fleet", g_fleet.toJson()}, {"mutex", g_arbiter.toJson()}};
    if (g_shift) state["shift"] = g_shift->snapshot();
    return state;
}

// -------------------------------------------------------------------------
// NHAT KY SU KIEN HE THONG (moi hanh dong server thuc hien: them/xoa xe, giao
// task, khoa node, luu ban do, loi xe, mat ket noi...). Duoc phat kem trong
// moi lan broadcastState() (truong "logs") de dashboard hien thi thong bao
// thoi gian thuc; Node.js lai luu ben ngoai vao SQLite theo "seq" de tranh
// luu trung khi cung 1 muc duoc gui lai nhieu lan (broadcast lap lai ~700ms).
// -------------------------------------------------------------------------
struct LogEntry { long seq; string time; string msg; };
static mutex        g_logMtx;
static deque<LogEntry> g_logs;      // gioi han LOG_MAX muc gan nhat trong bo nho
static long          g_logSeq = 0;
static const size_t  LOG_MAX = 300; // luu trong RAM; lich su day du nam o SQLite (Node.js)
static const size_t  LOG_BROADCAST = 20; // chi gui N muc gan nhat moi lan de nhe payload

static string nowStr() {
    time_t t = time(nullptr); tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
    return string(buf);
}

// Ghi 1 su kien vao nhat ky (thread-safe). Goi tu ca luong MQTT lan luong WebSocket.
static void pushLog(const string& msg) {
    lock_guard<mutex> lk(g_logMtx);
    g_logs.push_back(LogEntry{ ++g_logSeq, nowStr(), msg });
    if (g_logs.size() > LOG_MAX) g_logs.pop_front();
}

static json recentLogsJson() {
    lock_guard<mutex> lk(g_logMtx);
    json arr = json::array();
    size_t start = g_logs.size() > LOG_BROADCAST ? g_logs.size() - LOG_BROADCAST : 0;
    for (size_t i = start; i < g_logs.size(); ++i)
        arr.push_back({ {"seq", g_logs[i].seq}, {"time", g_logs[i].time}, {"msg", g_logs[i].msg} });
    return arr;
}

void publishTo(const string& topic, const string& payload) {
    if (g_mqttConnected && g_storageHealthy) g_outbound.push_back({topic,payload});
}
static void checkpointAndPublish(bool attemptRecovery = false) {
    if (!g_storageHealthy && !attemptRecovery) {g_outbound.clear();return;}
    const bool recovering = !g_storageHealthy;
    try {
        if (g_runtime) g_runtime->save({{"version",1},{"map",g_map.toJson()},
            {"fleet",g_fleet.durableSnapshot()},{"mutex",g_arbiter.toJson()}});
    } catch (const std::exception& e) {
        g_storageHealthy = false;g_outbound.clear();g_fleet.setNetworkAvailable(false);
        pushLog(string("[LOI] Checkpoint: ")+e.what());throw;
    }
    if (recovering) {
        g_storageHealthy = true;
        g_fleet.setNetworkAvailable(g_mqttConnected.load());
        pushLog("Checkpoint da ghi lai thanh cong; MQTT/heartbeat hoat dong lai, khong phat lai tuyen bi gian doan");
    }
    for (const auto& item : g_outbound) if (g_mosq && g_mqttConnected)
        mosquitto_publish(g_mosq,NULL,item.first.c_str(),static_cast<int>(item.second.size()),item.second.c_str(),0,false);
    g_outbound.clear();
}

// =========================================================================
// PHẦN B — WEBSOCKET SERVER TỐI GIẢN (Winsock2 + CNG SHA1/Base64)
// Ghi chú: bản triển khai tham khảo cho LAN nội bộ, hỗ trợ frame text đơn
// (không phân mảnh), chưa hỗ trợ TLS (wss://). Đủ dùng cho dashboard giám
// sát/điều khiển AGV trong xưởng.
// =========================================================================
struct ClientConn {
    SOCKET sock = INVALID_SOCKET;
    mutex  sendMtx; // std::mutex khong the copy/move -> KHONG duoc copy struct nay,
                     // luon tao qua make_shared<ClientConn>() roi gan truong sau.
    json previous;
    unsigned long stateSeq = 0;
};
static vector<shared_ptr<ClientConn>> g_clients;
static mutex g_clientsMtx;

// Ma hoa Base64 bang OpenSSL (EVP_EncodeBlock) -- dung CHUNG 1 ham cho ca Windows
// va Linux/Raspberry Pi, thay the CryptBinaryToStringA (CryptoAPI, chi co tren Windows).
static string base64Encode(const unsigned char* data, int len) {
    string out((size_t)(((len + 2) / 3) * 4) + 1, '\0'); // cong 1 cho null-terminator EVP_EncodeBlock ghi kem
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]), data, len);
    out.resize((size_t)written);
    return out;
}

// SHA1 + Base64 cho bat tay WebSocket (RFC 6455) bang OpenSSL -- thay the BCrypt/CNG
// (CryptoAPI, chi co tren Windows), chay duoc tren ca Linux/Raspberry Pi.
static string sha1Base64(const string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH] = {0}; // 20 byte
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

static bool recvAll(SOCKET socket, void* buffer, size_t count) {
    char* bytes = static_cast<char*>(buffer);
    size_t done = 0;
    while (done < count) {
        int n = recv(socket, bytes + done, static_cast<int>(count - done), 0);
        if (n <= 0) return false;
        done += static_cast<size_t>(n);
    }
    return true;
}
static bool sendAll(SOCKET socket, const char* bytes, size_t count) {
    size_t done = 0;
    while (done < count) {
        int n = send(socket, bytes + done, static_cast<int>(count - done), 0);
        if (n <= 0) return false;
        done += static_cast<size_t>(n);
    }
    return true;
}
static bool wsHandshake(SOCKET client) {
    // Read to the header terminator; TCP can split one handshake across receives.
    string req;
    while (req.size() < 8192 && (req.size() < 4 || req.substr(req.size()-4) != "\r\n\r\n")) {
        char byte;
        if (!recvAll(client, &byte, 1)) return false;
        req.push_back(byte);
    }
    if (req.size() >= 8192 || req.find("Upgrade: websocket") == string::npos) return false;
    if (const char* token = std::getenv("AGV_CORE_TOKEN")) {
        const string header = "Authorization: Bearer "+string(token)+"\r\n";
        const auto auth = req.find("Authorization: Bearer ");
        if (auth == string::npos || req.size()-auth < header.size() ||
            CRYPTO_memcmp(req.data()+auth,header.data(),header.size()) != 0) {
            const string denied = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            sendAll(client,denied.data(),denied.size());return false;
        }
    }
    string marker = "Sec-WebSocket-Key:";
    size_t pos = req.find(marker);
    if (pos == string::npos) return false;
    pos += marker.size();
    size_t end = req.find("\r\n", pos);
    string key = req.substr(pos, end - pos);
    size_t s = key.find_first_not_of(" \t");
    size_t e = key.find_last_not_of(" \t");
    key = (s == string::npos) ? "" : key.substr(s, e - s + 1);

    string accept = sha1Base64(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                  "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return sendAll(client, resp.data(), resp.size());
}

static bool wsSendText(shared_ptr<ClientConn> c, const string& msg) {
    lock_guard<mutex> lk(c->sendMtx);
    vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + opcode text
    size_t len = msg.size();
    if (len <= 125) frame.push_back((unsigned char)len);
    else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF); frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) frame.push_back((unsigned char)((len >> (8 * i)) & 0xFF));
    }
    frame.insert(frame.end(), msg.begin(), msg.end());
    return sendAll(c->sock, reinterpret_cast<const char*>(frame.data()), frame.size());
}

// Đọc 1 frame text từ client (frame client->server luôn có mask theo chuẩn RFC6455)
static bool wsRecvText(shared_ptr<ClientConn> client, string& out, bool& closed) {
    out.clear(); closed = false;
    unsigned char hdr[2];
    if (!recvAll(client->sock, hdr, 2)) { closed = true; return false; }
    unsigned char opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7F;
    if (!(hdr[0] & 0x80) || (hdr[0] & 0x70) || !masked) { closed = true; return false; }
    if (len == 126) {
        unsigned char ext[2];
        if (!recvAll(client->sock, ext, 2)) { closed = true; return false; }
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (!recvAll(client->sock, ext, 8)) { closed = true; return false; }
        len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }
    if (len > 1024 * 1024 || (opcode >= 8 && len > 125)) { closed = true; return false; }
    unsigned char maskKey[4];
    if (!recvAll(client->sock, maskKey, 4)) { closed = true; return false; }
    string payload(static_cast<size_t>(len), '\0');
    if (!recvAll(client->sock, payload.data(), payload.size())) { closed = true; return false; }
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(static_cast<unsigned char>(payload[i]) ^ maskKey[i % 4]);
    if (opcode == 8) { closed = true; return false; }
    if (opcode == 1) { out = std::move(payload); return true; }
    if (opcode == 9) {
        lock_guard<mutex> lk(client->sendMtx);
        string pong; pong.push_back(static_cast<char>(0x8A)); pong.push_back(static_cast<char>(len)); pong += payload;
        return sendAll(client->sock, pong.data(), pong.size());
    }
    return opcode == 10; // Other opcodes/fragmentation are not supported; close rather than misparse.
}

// Xử lý 1 lệnh JSON gửi lên từ dashboard web
static json handleWebCommand(const string& msg) {
    lock_guard<recursive_mutex> lk(g_stateMtx);
    json result;
    try {
        auto request = json::parse(msg);
        const string cmd = request.value("cmd", "");
        if (!g_storageHealthy) result = {{"type","command_result"},{"request_id",request.value("request_id","")},{"ok",false},{"status",503},{"error","Checkpoint loi; core dung phat lenh"}};
        else if (!g_mqttConnected && (cmd == "add_task" || cmd == "direct_goto" || cmd == "clear_error" || cmd == "cancel_task"))
            result = {{"type","command_result"},{"request_id",request.value("request_id","")},{"ok",false},{"status",503},{"error","Mat ket noi MQTT broker"}};
        else result = g_commands.execute(request);
        if (cmd == "confirm_zone_clear" && result.value("ok",false)) {
            const auto data = result.at("data");const auto grant = g_arbiter.grant(data.at("zone").get<string>(),data.at("new_owner").get<string>());
            if (!grant.is_null()) publishTo("agv/"+data.at("new_owner").get<string>()+"/mutex_grant",grant.dump());
        }
        try {checkpointAndPublish();} catch (...) {
            result = {{"type","command_result"},{"request_id",request.value("request_id","")},{"ok",false},{"status",503},{"error","Khong luu duoc checkpoint; lenh khong duoc phat"}};
        }
        pushLog(result.value("ok", false) ? "Da xu ly lenh " + request.value("cmd", "")
                                          : "[LOI] " + result.value("error", "Lenh bi tu choi"));
    } catch (const std::exception& e) {
        result = {{"type", "command_result"}, {"request_id", ""}, {"ok", false}, {"status", 400}, {"error", e.what()}};
    }
    result["core_session_id"] = CORE_SESSION;
    result["state"] = stateSnapshot();
    result["state"]["logs"] = recentLogsJson();
    return result;
}

static void wsClientThread(shared_ptr<ClientConn> c) {
    if (!wsHandshake(c->sock)) { closesocket(c->sock); return; }
    // Gửi ngay snapshot hiện tại cho client mới kết nối
    json snap = stateSnapshot();
    snap["logs"] = recentLogsJson();
    c->previous = snap;snap["state_seq"] = ++c->stateSeq;
    wsSendText(c, snap.dump());
    {
        lock_guard<mutex> lk(g_clientsMtx);
        g_clients.push_back(c);
    }

    string msg; bool closed = false;
    while (wsRecvText(c, msg, closed)) {
        if (!msg.empty()) wsSendText(c, handleWebCommand(msg).dump());
    }
    { lock_guard<mutex> sendLock(c->sendMtx); closesocket(c->sock); c->sock = INVALID_SOCKET; }
    lock_guard<mutex> lk(g_clientsMtx);
    g_clients.erase(remove_if(g_clients.begin(), g_clients.end(),
                    [&](const shared_ptr<ClientConn>& x) { 
                        return x.get() == c.get(); }), g_clients.end());
}

static void wsAcceptLoop(SOCKET listenSock) {
    while (true) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        auto conn = make_shared<ClientConn>(); // tao rong, KHONG copy (mutex ben trong khong copy duoc)
        conn->sock = client;
#if defined(_WIN32)
        DWORD sendTimeout = 500;
#else
        timeval sendTimeout{0,500000};
#endif
        setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&sendTimeout),sizeof(sendTimeout));
        thread(wsClientThread, conn).detach();
    }
}

static void broadcastState(bool positionChanged = false) {
    lock_guard<recursive_mutex> stateLock(g_stateMtx);
    static auto last = Clock::time_point{};
    const auto now = Clock::now();if (!positionChanged && now-last < chrono::milliseconds(200)) return;
    last = now;
    json snap = stateSnapshot();
    snap["logs"] = recentLogsJson(); // nhat ky su kien gan nhat, xem "NHAT KY SU KIEN HE THONG" o Phan A
    vector<shared_ptr<ClientConn>> clientsCopy;
    { lock_guard<mutex> lk(g_clientsMtx); clientsCopy = g_clients; }
    for (auto& c : clientsCopy) {
        json changes = json::object();
        for (const auto& field : snap.items()) if (!c->previous.contains(field.key()) || c->previous[field.key()] != field.value()) changes[field.key()] = field.value();
        const json delta = {{"type","state_delta"},{"base_state_seq",c->stateSeq},{"state_seq",c->stateSeq+1},{"changes",changes}};
        if (wsSendText(c,delta.dump())) {c->previous = snap;++c->stateSeq;}
    }
}

static SOCKET createWebSocketListener(int port) {
    const string host = std::getenv("AGV_CORE_WS_HOST") ? std::getenv("AGV_CORE_WS_HOST") : "127.0.0.1";
    if (host != "127.0.0.1" && !std::getenv("AGV_CORE_TOKEN")) {
        cerr << "[WS] Core ngoai loopback can AGV_CORE_TOKEN\n";return INVALID_SOCKET;
    }
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { cerr << "[WS] WSAStartup that bai\n"; return INVALID_SOCKET; }
#endif

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        cerr << "[WS] Khong the tao socket lang nghe\n";
        return INVALID_SOCKET;
    }

    // Windows SO_REUSEADDR cho phép hai core cùng bind và tranh kết nối MQTT.
    // Giữ độc quyền listener trước khi đọc counter hoặc kết nối broker.
    int socketOption = 1;
#if defined(_WIN32)
    const int addressOption = SO_EXCLUSIVEADDRUSE;
#else
    const int addressOption = SO_REUSEADDR;
#endif
    if (setsockopt(listenSock, SOL_SOCKET, addressOption,
                   reinterpret_cast<const char*>(&socketOption), sizeof(socketOption)) == SOCKET_ERROR) {
        cerr << "[WS] Khong the cau hinh quyen su dung cong\n";
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = inet_addr(std::getenv("AGV_CORE_WS_HOST") ? std::getenv("AGV_CORE_WS_HOST") : "127.0.0.1"); addr.sin_port = htons((uint16_t)port);
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "[WS] Bind that bai tren port " << port << ". Kiem tra core khac dang chay; chi chay mot core tren cong nay.\n";
        closesocket(listenSock);
        return INVALID_SOCKET;
    }
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[WS] Listen that bai tren port " << port << "\n";
        closesocket(listenSock);
        return INVALID_SOCKET;
    }
    cout << "[WS] C++ Core WebSocket dang lang nghe tren ws://" << (std::getenv("AGV_CORE_WS_HOST") ? std::getenv("AGV_CORE_WS_HOST") : "127.0.0.1") << ":" << port << "\n";
    return listenSock;
}

// =========================================================================
// PHẦN C — MQTT (giao tiếp với AGV qua ESP32)
// =========================================================================
static bool parseTopic(const string& topic, string& agvId, string& action) {
    stringstream ss(topic); string part; vector<string> parts;
    while (getline(ss, part, '/')) parts.push_back(part);
    if (parts.size() != 3 || parts[0] != "agv" || parts[1].empty() || parts[2].empty() || topic.back() == '/') return false;
    if (!validIdentifier(parts[1])) return false;
    agvId = parts[1]; action = parts[2]; return true;
}

static void on_connect(mosquitto* mosq, void*, int rc) {
    lock_guard<recursive_mutex> stateLock(g_stateMtx);
    g_mqttConnected = rc == 0;
    g_fleet.setNetworkAvailable(rc == 0 && g_storageHealthy);
    static bool hasConnected= false;
    if (rc == 0) {
        if(!hasConnected){
        cout << "[MQTT] Ket noi Broker thanh cong! Dang lang nghe cac xe AGV...\n";
        hasConnected= true;
        }
        mosquitto_subscribe(mosq, NULL, "agv/+/telemetry", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/diagnostic", 0);
        mosquitto_subscribe(mosq, NULL, "agv/01/odometry", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/mutex_request", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/mutex_release", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/obstacle", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/error", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/charging_done", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/ack", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/mutex_ack", 0);
        mosquitto_subscribe(mosq, NULL, "agv/+/status", 0);
    } else {
        cerr << "[MQTT] Ket noi that bai, ma loi: " << rc << "\n";
        hasConnected= false;
    }
}

static void on_disconnect(mosquitto*, void*, int) {
    lock_guard<recursive_mutex> stateLock(g_stateMtx);
    g_mqttConnected = false;g_fleet.reportNetworkLoss();
    pushLog("[LOI] Mat ket noi MQTT broker");requestWake();
}
static long grantId(const json& value) {
    if (!value.at("grant_id").is_number_integer()) throw std::invalid_argument("grant_id phai la so nguyen");
    const auto id = value.at("grant_id").get<long long>();
    if (id < 1 || id > INT_MAX) throw std::invalid_argument("grant_id ngoai gioi han");
    return static_cast<long>(id);
}

static void on_message(mosquitto*, void*, const mosquitto_message* msg) {
    lock_guard<recursive_mutex> stateLock(g_stateMtx);
    try {
    bool positionChanged = false;

    string topic = msg->topic;
    if (msg->payloadlen < 0 || msg->payloadlen > 4096) return;
    string payload = msg->payloadlen ? string(static_cast<char*>(msg->payload), msg->payloadlen) : string{};
    string agvId, action;
    if (!parseTopic(topic, agvId, action)) return;
    if (msg->retain && action != "status") return;
    if (!g_storageHealthy) return;

    if (action == "odometry") {
        const auto sample = EncoderSample::parse(json::parse(payload));
        if (agvId == "01" && sample.coreSession == CORE_SESSION && g_fleet.observeOdometry(agvId,sample))
            broadcastState(true);
        return; // dữ liệu hiển thị không ghi checkpoint hoặc điều phối
    } else if (action == "telemetry") {
        // Cu phap: "RFID|battery" hoac "RFID|battery|obstacle(0/1)"
        vector<string> parts; stringstream ss(payload); string tok;
        while (getline(ss, tok, '|')) parts.push_back(tok);
        if ((parts.size() == 2 || parts.size() == 3) && !parts[0].empty() && !parts[1].empty() && payload.back() != '|') {
            string rfid = parts[0];
            if (!validIdentifier(rfid) || (rfid != "UNKNOWN" && !g_map.nodeExists(rfid))) throw std::invalid_argument("Node telemetry sai");
            const bool batteryUnknown = parts[1] == "?";
            if (!batteryUnknown && !all_of(parts[1].begin(), parts[1].end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) throw std::invalid_argument("Pin telemetry khong hop le");
            int battery = batteryUnknown ? -1 : stoi(parts[1]);
            if (battery > 100) throw std::invalid_argument("Pin telemetry ngoai 0..100");
            if (parts.size() == 3 && parts[2] != "0" && parts[2] != "1") throw std::invalid_argument("Co vat can khong hop le");
            bool obstacle = (parts.size() == 3 && parts[2] == "1");
            positionChanged = g_fleet.updateTelemetry(agvId, rfid, battery, obstacle);
            if (obstacle) requestWake(); // vat can bao qua telemetry -> replanning ngay, khong doi het 700ms
        } else throw std::invalid_argument("Telemetry phai la node|battery[|0/1]");
    } else if (action == "diagnostic") {
        const auto diagnostic = json::parse(payload);
        if (!diagnostic.is_object() || diagnostic.value("version", 0) != 1 ||
            diagnostic.value("agvId", string{}) != agvId ||
            !diagnostic.contains("batteryCalibrated") || !diagnostic["batteryCalibrated"].is_boolean()) return;

        int displayPercent = -1;
        if (diagnostic["batteryCalibrated"].get<bool>()) {
            const auto value = diagnostic.find("batteryPercentage");
            if (value == diagnostic.end() || !value->is_number()) return;
            const double percent = value->get<double>();
            if (!std::isfinite(percent) || percent < 0.0 || percent > 100.0) return;
            displayPercent = static_cast<int>(std::lround(percent));
        }
        bool encoderBound = false;
        if (diagnostic.value("encoderSupported",false) && g_fleet.encoderEnabled(agvId)) {
            if (diagnostic.value("encoderCoreSessionId",string{}) != CORE_SESSION) return;
            for (const auto* key : {"encoderBootId","session","uptimeMs"}) {
                if (!diagnostic.contains(key) || !diagnostic[key].is_number_integer() ||
                    diagnostic[key].get<int64_t>() < 0 || diagnostic[key].get<uint64_t>() > UINT32_MAX)
                    throw std::invalid_argument("Encoder diagnostic session sai");
            }
            if (!g_fleet.bindEncoderSession(agvId,diagnostic["encoderBootId"].get<uint32_t>(),
                diagnostic["session"].get<uint32_t>(),diagnostic["uptimeMs"].get<uint32_t>())) return;
            encoderBound = true;
        }
        g_fleet.updateBatteryDisplay(agvId, displayPercent);
        if (diagnostic.contains("node") && diagnostic["node"].is_string() &&
            diagnostic.contains("state") && diagnostic["state"].is_string() &&
            diagnostic.contains("error") && diagnostic["error"].is_string() &&
            diagnostic.contains("motor") && diagnostic["motor"].is_string() &&
            diagnostic.contains("taskActive") && diagnostic["taskActive"].is_boolean() &&
            diagnostic.contains("wifi") && diagnostic["wifi"].is_boolean() &&
            diagnostic.contains("mqtt") && diagnostic["mqtt"].is_boolean() &&
            diagnostic.contains("leftDuty") && diagnostic["leftDuty"].is_number_integer() &&
            diagnostic.contains("rightDuty") && diagnostic["rightDuty"].is_number_integer()) {
                MotionDiagnostic motion;
                motion.node = diagnostic["node"].get<string>();
                motion.state = diagnostic["state"].get<string>();
                motion.error = diagnostic["error"].get<string>();
                motion.motor = diagnostic["motor"].get<string>();
                motion.taskActive = diagnostic["taskActive"].get<bool>();
                motion.wifi = diagnostic["wifi"].get<bool>();
                motion.mqtt = diagnostic["mqtt"].get<bool>();
                if (diagnostic.contains("rfidAgeMs") && diagnostic["rfidAgeMs"].is_number_integer()) {
                    const auto age = diagnostic["rfidAgeMs"].get<long long>();
                    if (age >= 0 && age < UINT32_MAX) motion.rfidAgeMs = static_cast<std::uint32_t>(age);
                }
                motion.leftDuty = diagnostic["leftDuty"].get<int>();
                motion.rightDuty = diagnostic["rightDuty"].get<int>();
                motion.headingSupported = diagnostic.value("headingSupported",false);
                motion.stationDockingSupported = diagnostic.value("stationDockingSupported",false);
                motion.stationReady = diagnostic.value("stationReady",false);
                motion.nodeOrdersSupported = diagnostic.value("nodeOrdersSupported",false);
                motion.straightThroughSupported = diagnostic.value("straightThroughSupported",false);
                motion.awaitingNodeOrder = diagnostic.value("awaitingNodeOrder",false);
                motion.stationTurnRequired = diagnostic.value("stationTurnRequired",false);
                motion.routeCommandId = diagnostic.value("routeCommandId",0L);
                motion.pathIndex = diagnostic.value("pathIndex",-1);
                motion.nextNode = diagnostic.value("nextNode",string{});
                motion.heading = diagnostic.value("heading",string{});
                motion.headingNode = diagnostic.value("headingNode",string{});
                motion.headingSource = diagnostic.value("headingSource",string{});
                if (encoderBound && agvId == "01" && diagnostic.contains("obstacle") && diagnostic["obstacle"].is_boolean() &&
                    diagnostic.contains("ultrasonicCm") &&
                    diagnostic.contains("obstacleDirection") && diagnostic["obstacleDirection"].is_string()) {
                    const double cm = diagnostic["ultrasonicCm"].is_number() ? diagnostic["ultrasonicCm"].get<double>() : -1;
                    const string direction = diagnostic["obstacleDirection"].get<string>();
                    if ((!diagnostic["obstacle"].get<bool>() || cm >= 0) && std::isfinite(cm) && cm <= 500 &&
                        (direction == "front" || direction == "rear")) {
                        motion.sonarPresent = true;
                        motion.sonarObstacle = diagnostic["obstacle"].get<bool>();
                        motion.sonarDistanceCm = cm;
                        motion.sonarDirection = direction;
                    }
                }
                g_fleet.observeMotionDiagnostic(agvId,motion);
                if (encoderBound) g_fleet.observeEncoderFirmwareFault(agvId,motion);
        }
    } else if (action == "mutex_request") {
        auto zones = g_arbiter.toJson();
        auto fleet = g_fleet.toJson();
        const auto selected = fleet.value("trial_robot_id",string{});
        if (!selected.empty() && selected != agvId) { pushLog("[LOI] Xe khong duoc chon xin mutex"); return; }
        bool online = false;
        for (const auto& robot : fleet["agvs"]) if (robot["id"] == agvId && robot.value("online", false) && robot.value("position_valid", false) && robot["state"] != "ERROR") online = true;
        if (!zones.contains(payload) || !online) { pushLog("[LOI] Mutex request khong hop le"); return; }
        if (g_arbiter.requestZone(payload, agvId)) {
            const auto grant = g_arbiter.grant(payload,agvId);
            if (!grant.is_null()) publishTo("agv/"+agvId+"/mutex_grant",grant.dump());
        }
    } else if (action == "mutex_ack") {
        const auto grant = json::parse(payload);
        g_arbiter.acknowledge(grant.at("zone").get<string>(),agvId,grantId(grant));
    } else if (action == "mutex_release") {
        const auto release = json::parse(payload);
        const auto zone = release.at("zone").get<string>();
        string nextAgv = g_arbiter.releaseZone(zone, agvId,grantId(release));
        if (!nextAgv.empty()) {
            const auto grant = g_arbiter.grant(zone,nextAgv);
            if (!grant.is_null()) publishTo("agv/"+nextAgv+"/mutex_grant",grant.dump());
        }
    } else if (action == "status") {
        if (payload == "offline") {g_fleet.reportOffline(agvId);requestWake();}
        else if (payload != "online") throw std::invalid_argument("Status MQTT sai");
    } else if (action == "obstacle") {
        g_fleet.reportObstacle(agvId, payload);
        cout << "[OBSTACLE] AGV_" << agvId << " phat hien vat can tai " << payload << "\n";
        pushLog("AGV_" + agvId + " phat hien vat can tai node " + payload);
        requestWake(); // danh thuc vong lap chinh ngay -- xem "CO CHE HUONG SU KIEN" o dau file
    } else if (action == "error") {
        g_fleet.reportError(agvId, payload);
        cout << "[ALARM] AGV_" << agvId << " BAO LOI: " << payload << "\n";
        requestWake(); // gián đoạn nhiệm vụ và broadcast ngay
    } else if (action == "charging_done") {
        // payload rong -> dung target mac dinh; neu ESP32 gui kem % pin that thi dung so do
        int finalBattery = -1;
        if (!payload.empty()) {
            if (!all_of(payload.begin(),payload.end(),[](unsigned char c){return c >= '0' && c <= '9';})) throw std::invalid_argument("charging_done sai");
            finalBattery = stoi(payload);if (finalBattery > 100) throw std::invalid_argument("charging_done ngoai 0..100");
        }
        g_fleet.reportCharged(agvId, finalBattery);
        cout << "[CHARGE] AGV_" << agvId << " da sac xong.\n";
        pushLog("AGV_" + agvId + " da sac xong (" + (finalBattery >= 0 ? to_string(finalBattery) : string("~90")) + "% pin)");
    } else if (action == "ack") {
        if (payload.empty() || !all_of(payload.begin(),payload.end(),[](unsigned char c){return c >= '0' && c <= '9';})) throw std::invalid_argument("ACK sai");
        g_fleet.reportAck(agvId, stol(payload));
    }
    checkpointAndPublish();
    // Actual RFID changes reach subscribers immediately after durable commit.
    // Do not wait for the background tick; unchanged heartbeat stays throttled.
    if (positionChanged) broadcastState(true);
    } catch (const std::exception& e) { pushLog(string("[LOI] MQTT: ") + e.what()); }
}

static bool startMqtt(const string& brokerIp, int port) {
    mosquitto_lib_init();
    g_mosq = mosquitto_new("AGV_MasterServer", true, NULL);
    if (!g_mosq) { cerr << "[MQTT] Khong the khoi tao mosquitto client!\n"; return false; }
    mosquitto_connect_callback_set(g_mosq, on_connect);
    mosquitto_disconnect_callback_set(g_mosq, on_disconnect);
    mosquitto_message_callback_set(g_mosq, on_message);
    const char* user = std::getenv("AGV_MQTT_USER"), *password = std::getenv("AGV_MQTT_PASSWORD");
    if (user && (!password || mosquitto_username_pw_set(g_mosq,user,password) != MOSQ_ERR_SUCCESS)) return false;
    const char* ca = std::getenv("AGV_MQTT_CA");
    if (ca && mosquitto_tls_set(g_mosq,ca,NULL,std::getenv("AGV_MQTT_CERT"),std::getenv("AGV_MQTT_KEY"),NULL) != MOSQ_ERR_SUCCESS) return false;
    if (ca && mosquitto_tls_opts_set(g_mosq,1,"tlsv1.2",NULL) != MOSQ_ERR_SUCCESS) return false;
    if (std::getenv("AGV_SECURITY_REQUIRED") && (!ca || !user || !password)) {
        cerr << "[MQTT] Che do secure can CA va tai khoan\n";return false;
    }
    if (mosquitto_connect(g_mosq, brokerIp.c_str(), port, 60) != MOSQ_ERR_SUCCESS) {
        cerr << "[MQTT] Khong the ket noi den Broker tai " << brokerIp << ":" << port << "\n";
        return false;
    }
    mosquitto_loop_start(g_mosq);
    return true;
}

// =========================================================================
// PHẦN D — MAIN
// =========================================================================
static bool enabled(const char* name) {
    const auto value = std::getenv(name);
    return value && string(value) == "1";
}

static string startupMode(int argc, char** argv, bool& quit) {
    string mode = std::getenv("AGV_TEST_MODE") ? std::getenv("AGV_TEST_MODE") : "";
    bool menu = false;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--select-mode") menu = true;
        else if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else throw std::invalid_argument("Dung --select-mode hoac --mode robot01|robot02|both|normal");
    }
#if defined(_WIN32)
    const bool terminal = _isatty(_fileno(stdin)) != 0;
#else
    const bool terminal = isatty(STDIN_FILENO) != 0;
#endif
    const bool configured = enabled("AGV_ONE_ROBOT_TRIAL") || enabled("AGV_TWO_ROBOT_TRIAL") || enabled("AGV_MONITOR_ONLY_02");
    if (menu || (mode.empty() && terminal && !configured)) {
        cout << "\n========== CHON CHE DO CORE AGV ==========\n"
             << "  1. Thu rieng xe 01 (tram S1, mutex bat)\n"
             << "  2. Thu rieng xe 02 (tram S2, mutex bat)\n"
             << "  3. Thu ca hai xe 01 + 02 (mutex bat)\n"
             << "  4. Che do van hanh thuong\n"
             << "  0. Thoat\n"
             << "Chon che do khong tu mo ca/cho xe chay.\n";
        string choice;
        for (;;) {
            cout << "Nhap lua chon (0-4), roi Enter: " << flush;
            if (!getline(cin,choice) || !g_running) {quit = true;return "";}
            stringstream input(choice);string token, extra;input >> token;
            if (!(input >> extra)) {
                if (token == "0") {quit = true;return "";}
                if (token == "1") {mode = "robot01";break;}
                if (token == "2") {mode = "robot02";break;}
                if (token == "3") {mode = "both";break;}
                if (token == "4") {mode = "normal";break;}
            }
            cout << "Lua chon khong hop le. Nhap 1, 2, 3, 4 hoac 0.\n";
        }
    }
    if (!mode.empty() && mode != "robot01" && mode != "robot02" && mode != "both" && mode != "normal")
        throw std::invalid_argument("Che do khong hop le: " + mode);
    if (!mode.empty()) cout << "[MODE] " << mode << "\n" << flush;
    return mode;
}

int main(int argc, char** argv) {
    if (const char* token = std::getenv("AGV_CORE_TOKEN")) {
        if (strlen(token) < 32 || string(token).find_first_of("\r\n") != string::npos) throw std::invalid_argument("AGV_CORE_TOKEN can >=32 ky tu");
    }
    // Bat Ctrl+C (SIGINT) / lenh kill (SIGTERM) de thoat vong lap chinh sach se tren
    // MOI nen tang, thay the GetAsyncKeyState(VK_ESCAPE) truoc day chi dung tren Windows.
    std::signal(SIGINT, onShutdownSignal);
    std::signal(SIGTERM, onShutdownSignal);
#if !defined(_WIN32)
    std::signal(SIGPIPE,SIG_IGN);
#endif

    bool quit = false;
    string mode;
    try { mode = startupMode(argc,argv,quit); }
    catch (const std::exception& e) { cerr << "[MODE] " << e.what() << "\n";return 1; }
    if (quit) return 0;

    // Giữ cổng trước mọi ghi map/counter và MQTT: bản chạy trùng phải thoát
    // mà không đẩy core đang chạy khỏi broker có cùng client ID.
    const SOCKET listenSock = createWebSocketListener(WS_PORT);
    if (listenSock == INVALID_SOCKET) {
        cerr << "[LOI] Khong khoi dong duoc WebSocket server.\n";
        return 1;
    }

    // 0) Bao dam thu muc data/ ton tai -- day la noi CO DINH server luu/nap ban do,
    //    thay vi thu muc lam viec hien tai cua tien trinh (thay doi tuy cach chay .exe,
    //    dan den nguoi dung khong biet "luu vao dau"). Duong dan: data/map_layout.json.
    static const string MAP_SAVE_DIR  = "data";
    const string MAP_SAVE_PATH = MAP_PATH;
    if (!std::filesystem::path(MAP_SAVE_PATH).parent_path().empty())
        std::filesystem::create_directories(std::filesystem::path(MAP_SAVE_PATH).parent_path()); // tao thu muc, bo qua neu da ton tai san (cross-platform)

    // 1) Nap ban do: uu tien data/map_layout.json (duong dan chuan tu ban nay tro di).
    //    Neu chua co (VD: file cu luu o thu muc goc tu ban truoc), thu nap file cu do
    //    roi LUU LAI ngay vao data/ de tu dong "don" ve 1 noi duy nhat. Neu khong co gi
    //    ca thi dung map demo khop so do nha xuong da thiet ke.
    try {
        g_map.loadFromFile(MAP_SAVE_PATH);
        cout << "[MAP] Da nap " << MAP_SAVE_PATH << "\n";
    } catch (const std::exception& e) {
        if (std::filesystem::exists(MAP_SAVE_PATH)) {
            cerr << "[MAP] Ban do luu khong hop le, giu nguyen file va dung core: " << e.what() << "\n";
            return 1;
        }
        try {
            g_map.loadFromFile("map_layout.json"); // duong dan cu (tuong thich nguoc)
            cout << "[MAP] Da nap map_layout.json (duong dan cu) -> se luu lai vao " << MAP_SAVE_PATH << "\n";
            g_map.saveToFile(MAP_SAVE_PATH);
        } catch (const std::exception&) {
            cout << "[MAP] Khong tim thay ban do da luu -> dung map demo mac dinh.\n";
            g_map.loadDefaultDemoMap();
        }
    }
    for (auto& z : g_map.mutexZones()) g_arbiter.registerZone(z);

    // 2) Nối FleetManager với kênh phát lệnh MQTT + kênh báo thay đổi -> WebSocket
    auto counterPath = std::getenv("AGV_COMMAND_ID_PATH") ? std::filesystem::path(std::getenv("AGV_COMMAND_ID_PATH"))
        : std::filesystem::path(MAP_SAVE_PATH).parent_path() / "command_sequence.json";
    // Khi chuyển từ chạy trong src sang gốc workspace, tiếp tục chuỗi ID cũ.
    // Map/counter do môi trường chỉ định vẫn dùng đúng đường dẫn đã cấu hình.
    const std::filesystem::path legacyCounter = "src/data/command_sequence.json";
    if (!std::getenv("AGV_COMMAND_ID_PATH") && !std::getenv("AGV_MAP_PATH") &&
        !std::filesystem::exists(counterPath) && std::filesystem::exists(legacyCounter))
        counterPath = legacyCounter;
    cout << "[COMMAND] Counter: " << std::filesystem::absolute(counterPath).string() << "\n";
    CommandIds commandIds(counterPath);
    const auto taskCounterPath = std::getenv("AGV_TASK_ID_PATH") ? std::filesystem::path(std::getenv("AGV_TASK_ID_PATH"))
        : std::filesystem::path(MAP_SAVE_PATH).parent_path()/"task_sequence.json";
    CommandIds taskIds(taskCounterPath);
    g_fleet.setTaskIdAllocator([&taskIds] { return "task-"+to_string(taskIds.next()); });
    g_arbiter.setGrantIdAllocator([&commandIds] { return commandIds.next(); });
    g_fleet.setCommandIdAllocator([&commandIds] { return commandIds.next(); });
    g_fleet.setPublishCallback(publishTo);
    g_fleet.setStateChangedCallback(requestWake);
    g_fleet.setChargeThresholds(20, 40, 90); // <=20% -> di sac; can >=40% moi nhan task moi; sac xong ~90% neu ESP32 khong bao pin that
    const bool singleRobotTrial = mode.empty() && enabled("AGV_ONE_ROBOT_TRIAL");
    const bool twoRobotTrial = mode.empty() ? enabled("AGV_TWO_ROBOT_TRIAL") : mode == "both";
    const bool monitorRobot02 = mode.empty() && enabled("AGV_MONITOR_ONLY_02");
    const string trialRobot = mode == "robot01" ? "01" : mode == "robot02" ? "02" : "";
    if (twoRobotTrial && (singleRobotTrial || monitorRobot02))
        throw std::invalid_argument("Khong ket hop che do thu hai xe voi thu mot xe/giam sat xe 02");
    g_fleet.setSingleRobotTrial(singleRobotTrial);
    g_fleet.setTwoRobotTrial(twoRobotTrial);
    g_fleet.setTrialRobot(trialRobot);
    if (monitorRobot02) g_fleet.setMonitorOnlyRobot("02");
    g_fleet.setOfflineTimeout(10);    // >10s không telemetry -> OFFLINE, task gián đoạn, giữ vùng
    g_fleet.setAckTimeout(3000);             // 3s khong thay ACK lenh -> gui lai
    g_fleet.setMaxRetries(3);                // gui lai toi da 3 lan truoc khi coi la mat lien lac lenh
    g_fleet.setEventCallback([](const string& msg) { cout << "[FLEET] " << msg << "\n"; pushLog(msg); });
    const auto runtimePath = std::getenv("AGV_RUNTIME_STATE_PATH") ? std::filesystem::path(std::getenv("AGV_RUNTIME_STATE_PATH"))
        : std::filesystem::path(MAP_SAVE_PATH).parent_path()/"runtime_state.json";
    g_runtime = std::make_unique<RuntimeStore>(runtimePath);
    if (const auto saved = g_runtime->load()) {
        if (!RuntimeStore::mapsCompatible(g_map.toJson(),saved->at("map"))) throw std::runtime_error("Runtime checkpoint khong khop map; can doi soat, khong tu xoa file");
        g_map.loadFromJson(saved->at("map"));g_arbiter.restore(saved->at("mutex"));g_fleet.restore(saved->at("fleet"));
        pushLog("Da khoi phuc queue/mutex; robot cho RFID va xac minh");
    }
    std::unordered_map<string,string> homes = {{"01","S1"},{"02","S2"}};
    if (std::getenv("AGV_HOME_STATIONS")) homes = json::parse(std::getenv("AGV_HOME_STATIONS")).get<decltype(homes)>();
    if (singleRobotTrial) homes = {{"01","S1"}};
    if (!trialRobot.empty()) homes = {{trialRobot,trialRobot == "01" ? "S1" : "S2"}};
    if (!mode.empty() && trialRobot.empty() && twoRobotTrial) homes = {{"01","S1"},{"02","S2"}};
    const auto shiftFile = std::getenv("AGV_SHIFT_STATE_PATH") ? std::filesystem::path(std::getenv("AGV_SHIFT_STATE_PATH"))
        : std::filesystem::path(MAP_SAVE_PATH).parent_path()/"shift_state.json";
    g_shift = std::make_unique<ShiftController>(g_map,g_fleet,g_arbiter,shiftFile,homes,singleRobotTrial,twoRobotTrial,trialRobot);
    if ((std::getenv("AGV_HOME_STATIONS") || !mode.empty()) && !homes.empty() && !g_shift->supported())
        throw std::invalid_argument("AGV_HOME_STATIONS phai dung robot ID va tram sac hop le/khong trung");
    g_commands.setShiftController(g_shift.get());
    g_commands.setSimulationMode(std::getenv("AGV_SIMULATION") && std::string(std::getenv("AGV_SIMULATION")) == "1");
    if (!g_shift->supported()) g_fleet.setAcceptingTasks(true);
    checkpointAndPublish();
    cout << "[SHIFT] " << g_shift->snapshot().dump() << "\n";

    // 3) Khởi động MQTT (giao tiếp AGV) — chỉnh IP broker tại đây nếu cần
    const string broker = std::getenv("AGV_MQTT_HOST") ? std::getenv("AGV_MQTT_HOST") : "127.0.0.1";
    const int mqttPort = envPort("AGV_MQTT_PORT", 1883);
    if (!startMqtt(broker, mqttPort)) {
        cerr << "[LOI] Khong khoi dong duoc MQTT, thoat chuong trinh.\n";
        return 1;
    }

    // 4) Chỉ nhận lệnh WebSocket sau khi map/counter/callback đã sẵn sàng.
    thread(wsAcceptLoop, listenSock).detach();

    cout << "\n=== AGV MASTER SERVER DANG CHAY ===\n";
    cout << "MQTT broker : " << broker << ":" << mqttPort << "\n";
    cout << "C++ Core   : ws://<IP-may-nay>:" << WS_PORT << "\n";
    cout << "Ban do luu tai: " << MAP_SAVE_PATH << " (thu muc lam viec hien tai: xem the tren)\n";
    cout << "Nhan Ctrl+C de thoat.\n\n";

    // 5) Vòng lặp chính: tick() định kỳ (dispatch + kiểm tra pin), thoát bằng Ctrl+C (g_running).
    //    Dùng condition_variable thay vì sleep_for cố định: chờ tối đa 700ms NHƯNG được đánh
    //    thức SỚM HƠN ngay khi luồng MQTT gọi requestWake() (vật cản/lỗi từ ESP32) -- loại bỏ
    //    độ trễ tĩnh khi cần replanning gấp, mà vẫn giữ 700ms làm chu kỳ tick() nền bình thường.
    auto lastStorageRetry = chrono::steady_clock::now();
    while (g_running) {
        { lock_guard<recursive_mutex> stateLock(g_stateMtx);
          try {
            if (!g_storageHealthy && chrono::steady_clock::now()-lastStorageRetry >= chrono::seconds(2)) {
                lastStorageRetry = chrono::steady_clock::now();
                checkpointAndPublish(true);
            }
            if (g_storageHealthy) {
                g_fleet.tick(); if (g_shift && g_mqttConnected) g_shift->tick();
                if (g_mqttConnected) {
                    for (const auto& grant : g_arbiter.retryGrants()) publishTo("agv/"+grant.first+"/mutex_grant",grant.second.dump());
                    publishTo("agv/server/heartbeat",CORE_SESSION);
                }
                checkpointAndPublish();
            }
          } catch (const std::exception& e) {cerr << "[RUNTIME] " << e.what() << "\n";}
          broadcastState(); }

        std::unique_lock<std::mutex> wakeLock(g_wakeMtx);
        g_wakeCv.wait_for(wakeLock, chrono::milliseconds(200),
                           [] { return g_wakeRequested.load() || !g_running.load(); });
        g_wakeRequested = false;
    }
    cout << "[CONTROL] Dang thoat chuong trinh...\n";

    // 6) Dọn dẹp
    mosquitto_loop_stop(g_mosq, true);
    mosquitto_disconnect(g_mosq);
    mosquitto_destroy(g_mosq);
    mosquitto_lib_cleanup();
    closesocket(listenSock);
#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
