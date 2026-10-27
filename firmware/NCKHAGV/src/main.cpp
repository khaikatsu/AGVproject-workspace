#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "core/AGVController.h"

namespace { AGVController agv; }

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Vô hiệu hóa ngắt sụt áp nguồn (brownout) khi khởi động Wi-Fi
    Serial.begin(Config::SERIAL_BAUD);
    agv.begin();
}
void loop() {
    agv.update();
    yield();
}
