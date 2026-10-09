#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <mosquitto.h>

using namespace std;

// Biến toàn cục quản lý Mosquitto Client
struct mosquitto* g_mosq = nullptr;
bool isConnectedToAGV = false; // Cờ kiểm tra trạng thái kết nối

// Hàm tách topic để lấy ID của xe (VD: agv/1/telemetry -> agvId = 1, action = telemetry)
bool parseTopic(const string& topic, string& agvId, string& action) {
    stringstream ss(topic); string part; vector<string> parts;
    while (getline(ss, part, '/')) parts.push_back(part);
    if (parts.size() != 3 || parts[0] != "agv") return false;
    agvId = parts[1]; action = parts[2]; return true;
}

// Callback khi Server C++ kết nối thành công với Mosquitto Broker trên máy
void on_connect(struct mosquitto* mosq, void* obj, int rc) {
    if (rc == 0) {
        cout << "[MQTT] Server da ket noi toi Broker (127.0.0.1) thanh cong!\n";
        cout << "[MQTT] Dang lang nghe tin hieu tu cac xe AGV...\n\n";
        // Đăng ký nhận dữ liệu từ tất cả các xe AGV
        mosquitto_subscribe(mosq, NULL, "agv/+/telemetry", 0);
    } else {
        cout << "[MQTT] Ket noi Broker that bai, ma loi: " << rc << "\n";
    }
}

// Callback xử lý khi có tin nhắn từ ESP32 gửi lên
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
    string topic = msg->topic;
    string payload(static_cast<char*>(msg->payload), msg->payloadlen);
    string agvId, action;

    if (!parseTopic(topic, agvId, action)) return;

    // Khi nhận được dữ liệu telemetry từ ESP32 gửi lên
    if (action == "telemetry") {
        if (!isConnectedToAGV) {
            isConnectedToAGV = true;
            cout << "\n==================================================\n";
            cout << " [SUCCESS] Da ket noi thanh cong voi ESP32 (AGV_" << agvId << ") qua MQTT!\n";
            cout << "==================================================\n";
        }
        
        cout << "[NHAN DU LIEU] AGV_" << agvId << " gui ve -> Noi dung: " << payload << endl;
    }
}

int main() {
    // 1. Khởi tạo thư viện Mosquitto
    mosquitto_lib_init();
    g_mosq = mosquitto_new("AGV_Master_Server", true, NULL);

    if (!g_mosq) {
        cerr << "[LOI] Khong the khoi tao Mosquitto client!\n";
        return 1;
    }

    // 2. Gắn các hàm bắt sự kiện
    mosquitto_connect_callback_set(g_mosq, on_connect);
    mosquitto_message_callback_set(g_mosq, on_message);

    // 3. Kết nối tới Broker chạy ở local (127.0.0.1)
    if (mosquitto_connect(g_mosq, "127.0.0.1", 1883, 60) != MOSQ_ERR_SUCCESS) {
        cerr << "[LOI] Khong the ket noi den MQTT Broker!\n";
        return 1;
    }

    cout << "=== HETHONG SERVER DIỀU KHIỂN AGV ĐANG CHẠY ===" << endl;
    cout << "Dang cho tin hieu tu ESP32... (Nhan Ctrl+C de thoat)\n" << endl;

    // 4. Chạy vòng lặp lắng nghe ngầm (Blocking loop - thay thế while(true) gây ngốn CPU)
    // Hàm này sẽ block luồng chính lại và tự động quản lý kết nối, keep-alive, gọi callback...
    mosquitto_loop_forever(g_mosq, -1, 1);

    // Dọn dẹp tài nguyên (khi thoát)
    mosquitto_loop_stop(g_mosq, true);
    mosquitto_disconnect(g_mosq);
    mosquitto_destroy(g_mosq);
    mosquitto_lib_cleanup();

    return 0;
}