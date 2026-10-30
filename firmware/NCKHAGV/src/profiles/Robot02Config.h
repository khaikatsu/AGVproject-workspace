#pragma once

// ============================================================================
// PROFILE CẤU HÌNH XE 02 (Môi trường build: esp32dev_two_robot_bench_02)
// ============================================================================
// Tất cả thông số điều khiển vận hành, cảm biến, động cơ và thuật toán cho Xe 02.
// Sử dụng thang PWM từ 0 đến 255.
// ============================================================================

#define AGV_PROFILE_ID "02"

// ----------------------------------------------------------------------------
// 1. CƠ CẤU BÀN NÂNG SERVO (Servo Lift Mechanism)
// ----------------------------------------------------------------------------
// Điều khiển servo nâng hạ pallet / kệ hàng (cấu hình đồng bộ giống Xe 01).
#define AGV_SERVO_MIN_PULSE_US 544      // Độ rộng xung nhỏ nhất của servo (microgiây)
#define AGV_SERVO_MAX_PULSE_US 2400     // Độ rộng xung lớn nhất của servo (microgiây)
#define AGV_SERVO_LIFT_DOWN_ANGLE 180   // Góc quay khi HẠ bàn nâng xuống (độ)
#define AGV_SERVO_LIFT_UP_ANGLE 100     // Góc quay khi NÂNG bàn nâng lên (độ)

// ----------------------------------------------------------------------------
// 2. THUẬT TOÁN BÁM LINE TIẾN BẰNG BỘ ĐIỀU KHIỂN PD (Line Following PD)
// ----------------------------------------------------------------------------
// Giúp xe bám line mượt mà, tự động điều tiết tốc độ hai bánh, chống rung lắc.
#define AGV_LINE_PD_ENABLED 1           // 1: Bật bám line bằng PD; 0: Dùng bảng điều khiển cơ bản
#define AGV_BENCH_LINE_KP 50.0f         // Hệ số tỉ lệ P: Lực kéo xe trở về tâm line (càng lớn kéo càng gắt)
#define AGV_BENCH_LINE_KD 40.0f         // Hệ số vi phân D: Dập tắt dao động lắc lư (càng lớn xe bẻ lái càng êm)
#define AGV_LINE_PD_BASE_SPEED 130      // Tốc độ chạy thẳng cơ sở (PWM 0..255) khi bám line tiến
#define AGV_LINE_PD_CONTROL_MS 10       // Chu kỳ tính toán thuật toán PD (ms, 10ms = 100Hz)
#define AGV_LINE_PD_RESET_GAP_MS 50     // Thời gian mất line (ms) trước khi reset tích lũy vi phân
#define AGV_LINE_PD_STEERING_SIGN 1     // Chiều lái bánh xe (+1: Bình thường; -1: Đảo chiều hai bánh)

// ----------------------------------------------------------------------------
// 3. TÌM LẠI LINE VÀ BẮT LINE SAU CUA (Line Recovery & Turn Capture)
// ----------------------------------------------------------------------------
#define AGV_LINE_LOSS_GRACE_MS 7000     // Thời gian tối đa (ms) chờ tìm lại line trước khi báo lỗi LINE_LOST

// Phục hồi khi xe vào cua chạy nhanh bị văng khỏi line (Corner Lost-Line Recovery):
#define AGV_CORNER_RECOVERY_ENABLED 1   // 1: Bật tự động cứu line khi vào cua; 0: Tắt
#define AGV_CORNER_RECOVERY_MS 350      // Chỉnh thời gian quay về line tại đây (ms, ví dụ 250 - 450ms)
#define AGV_CORNER_RECOVERY_PWM 165     // Tốc độ PWM bánh xe khi quay về bắt line
#include "SharedRobotConfig.h"          // Nạp cấu hình phần cứng chung

// Bắt lại line sau khi hoàn thành rẽ trái / phải:
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1 // 1: Bật cơ chế hãm bắt lại line sau khi cua trái
#define AGV_LEFT_TURN_CAPTURE_PWM 190    // Tốc độ PWM bánh xe để bắt lại line sau cua trái
#define AGV_RIGHT_TURN_CAPTURE_ENABLED 1// 1: Bật cơ chế hãm bắt lại line sau khi cua phải
#define AGV_RIGHT_TURN_CAPTURE_ENAB 160   // Tốc độ PWM bánh xe để bắt lại line sau cua phải

// ----------------------------------------------------------------------------
// 4. CHẠY THẲNG CƠ BẢN DỰ PHÒNG (Fallback Motor Speed)
// ----------------------------------------------------------------------------
// Dùng khi không kích hoạt PD hoặc trong các chặng chạy thẳng đặc biệt.
#define AGV_BENCH_MOTOR_SPEED 130       // Tốc độ chạy thẳng dự phòng (PWM)
#define AGV_BENCH_LEFT_SPEED_TRIM 0     // Bù lệch tốc độ bánh phải (kênh ENA)
#define AGV_BENCH_RIGHT_SPEED_TRIM 0    // Bù lệch tốc độ bánh trái (kênh ENB)

// ----------------------------------------------------------------------------
// 5. QUAY TẠI GIAO LỘ & XOAY ĐẦU 180 ĐỘ (Pivot Turning & Turn 180)
// ----------------------------------------------------------------------------
// Tốc độ quay tại chỗ: ENA/ENB = 210/200 sau khi trừ trim.
#define AGV_BENCH_TURN_SPEED 255        // Tốc độ PWM quay tại chỗ thông thường (góc 90 độ, hai bánh chạy ngược chiều)
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 0    // Bù tốc độ kênh ENA khi quay tại chỗ
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 0 // Bù tốc độ kênh ENB khi quay tại chỗ
#define AGV_BENCH_TURN_MIN_MS 700       // Thời gian tối thiểu quay góc 90 độ (ms) trước khi bắt line mới

// Riêng quay đầu 180 độ (tại G1/G2 hoặc quay đầu chặng):
// Toc do xoay 180 do rieng khi ve tram Base S2 (S2 auto-turn 180):
#define AGV_STATION_TURN_180_PWM 175

#define AGV_G1_TURN_AROUND_PWM (AGV_BENCH_TURN_SPEED - 50) // PWM quay 180 độ (giảm 30 PWM để chống trượt bánh)

// [CHỐNG ĂN LINE PHỤ KHI XOAY 180 ĐỘ]:
// Thời gian xe quay mù tối thiểu (ms) trong quá trình xoay 180 độ.
// Trong suốt thời gian này, xe ép quay tròn liên tục và BỎ QUA mọi vạch nhánh/line phụ ở giữa.
// Tăng lên (ví dụ 450 - 1000ms) nếu xe bị dừng sớm do ăn line phụ.
#define AGV_TURN_180_MIN_MS 800

// [PHANH DỪNG XÁC NHẬN SAU KHI XOAY 180 ĐỘ]:
// Sau khi đã xoay đủ góc và mắt đọc bắt trúng vạch line đích, xe phanh đứng yên trong bao lâu (ms)
// để triệt tiêu toàn bộ quán tính xoay và xác nhận chuẩn tâm 010 trước khi tiếp tục chạy.
#define AGV_TURN_AROUND_CONFIRM_MS 100  // Chuẩn 100ms (không nên để 1000ms vì xe sẽ bị đứng đơ 1 giây)

// [TỰ ĐỘNG CỨU LINE KHI XOAY 180 ĐỘ BỊ LỐ (Turn 180 Overshoot Recovery)]:
// Khi xe xoay 180 độ trớn quán tính quăng lướt qua khỏi vạch (vào 000 hoặc 110/100):
// Xe sẽ tự động quay ngược chiều (PivotLeft) để kéo mắt đọc về lại chuẩn tâm 010.
// Mặc định nạp từ SharedRobotConfig.h: Bật cứu line (1), tốc độ quay ngược 165 PWM, độ trễ phanh 100ms.
// Bỏ comment nếu muốn tinh chỉnh riêng cho Xe 02:
// #define AGV_TURN_180_RECOVERY_ENABLED 1
// #define AGV_TURN_180_RECOVERY_PWM 165
// #define AGV_TURN_180_RECOVERY_DELAY_MS 100

// ----------------------------------------------------------------------------
// 6. ĐIỀU KHIỂN DOCKING VÀO BASE S2 (GS2 -> S2 Reversing)
// ----------------------------------------------------------------------------
// Tốc độ xoay tại trạm GS2 khi chuẩn bị lùi vào base S2 (giảm tốc để bắt chuẩn tâm không văng trớn):
#define AGV_GS_TURN_PWM 170
// Tốc độ nhích vi chỉnh bắt chuẩn tâm '010' tại GS2 khi vào TurnCapture:
#define AGV_GS_ALIGN_PWM 155
// Thời gian hãm dừng xác nhận chuẩn tâm '010' tại GS2 trước khi bắt đầu lùi vào base (ms):
#define AGV_GS_CONFIRM_MS 100
// Thời gian tiến thêm bám line PD (ms) sau khi bắt line tại GS2 để nắn thẳng xe trước khi lùi:
// Khuyên dùng 200 - 300ms (nhích nhẹ ~5cm nắn thẳng bánh). Đặt 0 nếu muốn xe quay xong lùi ngay lập tức.
// Tuyệt đối không đặt quá 500ms vì nhánh line GS2 ngắn, xe sẽ chạy văng ra ngoài khoảng trống!
#define AGV_REVERSE_ALIGN_FORWARD_MS 600

// ----------------------------------------------------------------------------
// 7. BÁM LINE TIẾN CƠ BẢN (Khi không dùng PD)
// ----------------------------------------------------------------------------
#define AGV_BENCH_CORRECTION_SPEED 160        // Tốc độ vi chỉnh bẻ lái khi lệch khỏi tâm line
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 160 // Tốc độ vi chỉnh nhẹ (dự phòng)

// ----------------------------------------------------------------------------
// 8. ĐIỀU KHIỂN CHẠY LÙI (Reverse Line Following)
// ----------------------------------------------------------------------------
#define AGV_REVERSE_SPEED 130           // Tốc độ PWM khi lùi thẳng
#define AGV_REVERSE_CORRECTION_SPEED 115// Tốc độ PWM khi vi chỉnh nắn lệch lúc đang lùi theo line
#define AGV_REVERSE_LEFT_SPEED_TRIM  -20 // Bù tốc độ lùi kênh ENA (bánh phải)
#define AGV_REVERSE_RIGHT_SPEED_TRIM 0 // Bù tốc độ lùi kênh ENB (bánh trái)
#define AGV_REVERSE_GENTLE_CORRECTION 1 // Chế độ nắn lệch lùi: 0 = dừng 1 bánh để cua; 1 = cả 2 bánh đều lùi, 1 bánh chậm
#define AGV_REVERSE_CORRECTION_DELTA 10 // Độ giảm tốc bánh chậm khi nắn lệch lùi (áp dụng cho chế độ 1)

// ----------------------------------------------------------------------------
// 9. TÍNH NĂNG CHỐNG KẸT ĐỀ-BA (Startup Unstick Boost)
// ----------------------------------------------------------------------------
// Khi có lệnh chạy nhưng xung encoder chưa nhảy (xe bị kẹt ma sát đứng yên tại chỗ),
// firmware tự động cộng thêm lượng PWM này để kích xe lăn bánh tức thì.
// Ngay khi encoder nhảy xung (xe đã lăn bánh), PWM lập tức tự động trả về mức ban đầu.
#define AGV_STARTUP_BOOST_PWM 10          // Mức PWM khởi điểm khi vừa phát hiện kẹt (0..255; đặt 0 để tắt)
#define AGV_STARTUP_BOOST_DELAY_MS 150    // Thời gian chờ xe tự lăn bánh (ms) trước khi bắt đầu nhồi đề-ba
#define AGV_STARTUP_BOOST_STEP_PWM 8      // Độ dốc tăng lực: Mỗi nấc nhồi thêm bao nhiêu PWM (đặt 5..10 để tăng từ từ êm ái)
#define AGV_STARTUP_BOOST_INTERVAL_MS 60  // Khoảng cách thời gian (ms) giữa các nấc tăng (đặt 50..100ms để tăng chậm, không bị vọt giật)
#define AGV_STARTUP_BOOST_MAX_PWM 180     // Giới hạn PWM boost tối đa (chống vọt xe quá mạnh)

// ----------------------------------------------------------------------------
// 10. BÙ TỐC ĐỘ THEO BẬC THANG % PIN (Battery Step Speed Compensation)
// ----------------------------------------------------------------------------
// Giúp xe luôn giữ vận tốc ổn định xuyên suốt từ 100% xuống 20% pin (chống phóng nhanh lúc đầy, ỳ lúc yếu).
// Mặc định nạp từ SharedRobotConfig.h:
//   Pin >= 80%: -10 PWM
//   Pin 50% - 79%: 0 PWM (chuẩn đã căn chỉnh)
//   Pin 30% - 49%: +8 PWM
//   Pin < 30%: +15 PWM
// Bỏ comment các dòng dưới nếu muốn tinh chỉnh riêng cho Xe 02:
// #define AGV_BATTERY_SPEED_COMP_ENABLED 1
// #define AGV_BATTERY_STEP_HIGH_PERCENT 80.0f
// #define AGV_BATTERY_PWM_OFFSET_HIGH -10
// #define AGV_BATTERY_STEP_MID_PERCENT 50.0f
// #define AGV_BATTERY_PWM_OFFSET_NOMINAL 0
// #define AGV_BATTERY_STEP_LOW_PERCENT 30.0f
// #define AGV_BATTERY_PWM_OFFSET_MID 8
// #define AGV_BATTERY_PWM_OFFSET_LOW 15
