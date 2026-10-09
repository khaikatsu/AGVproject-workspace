#include "driver.h"
#include "Config.h"
#include "TestConfig.h"

#if defined(AGV_FIRMWARE_HOST) && !defined(AGV_DRIVER_BENCH)
namespace AgvDrive {
static bool running = false;
void begin() { running = false; }
void stop() { running = false; }
void tick(bool permitted) { running = permitted && TestConfig::ENABLE_AUTONOMOUS_MOTION; }
bool obstacle() { return false; }
bool lineFault() { return false; }
bool motorRunning() { return running; }
}
#else
#include <Arduino.h>

namespace {
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool echoWaiting = false, echoRiseSeen = false, echoReady = false;
volatile uint32_t echoRiseUs = 0, echoDurationUs = 0, echoTriggerUs = 0;
uint32_t lastPingMs = 0, lastEchoMs = 0, servoSinceMs = 0;
bool sonarBlocked = true, lineLost = true, running = false, servoAttached = false, motorPwmReady = false;
int8_t activeLineLevel = TestConfig::LINE_ACTIVE_LEVEL;
int servoAngle = 90, servoDirection = 1, pingAngle = 90;

void setSide(bool left, int speed) {
  const int trim = left ? TestConfig::LEFT_PWM_TRIM : TestConfig::RIGHT_PWM_TRIM;
  const bool invert = left ? TestConfig::LEFT_MOTOR_INVERTED : TestConfig::RIGHT_MOTOR_INVERTED;
  const int value = speed ? constrain(abs(speed) + trim, 0, 255) * (invert ? -1 : 1) : 0;
  const uint8_t a = left ? Config::LEFT_IN1 : Config::RIGHT_IN3;
  const uint8_t b = left ? Config::LEFT_IN2 : Config::RIGHT_IN4;
  const uint8_t en = left ? Config::LEFT_ENA : Config::RIGHT_ENB;
  digitalWrite(a, value > 0 ? HIGH : LOW);
  digitalWrite(b, value < 0 ? HIGH : LOW);
  ledcWrite(en, abs(value)); // ESP32 Arduino core 3.x uses pin-based LEDC.
}

void drive(int left, int right) {
  setSide(true, left);
  setSide(false, right);
  running = left != 0 || right != 0;
}

void IRAM_ATTR echoChangeIsr() {
  const bool high = digitalRead(Config::ULTRASONIC_ECHO_PIN);
  const uint32_t now = micros();
  portENTER_CRITICAL_ISR(&echoMux);
  if (echoWaiting) {
    if (high) { echoRiseUs = now; echoRiseSeen = true; }
    else if (echoRiseSeen) {
      echoDurationUs = now - echoRiseUs;
      echoReady = true;
      echoWaiting = false;
    }
  }
  portEXIT_CRITICAL_ISR(&echoMux);
}

void startPing() {
  pingAngle = servoAngle;
  portENTER_CRITICAL(&echoMux);
  echoWaiting = true;
  echoRiseSeen = echoReady = false;
  echoTriggerUs = micros();
  portEXIT_CRITICAL(&echoMux);
  digitalWrite(Config::ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(Config::ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(Config::ULTRASONIC_TRIG_PIN, LOW);
}

void pollSonar() {
  const uint32_t now = millis();
  uint32_t duration = 0;
  bool ready = false;
  portENTER_CRITICAL(&echoMux);
  if (echoReady) {
    duration = echoDurationUs;
    echoReady = false;
    ready = true;
  } else if (echoWaiting && uint32_t(micros() - echoTriggerUs) >= TestConfig::ECHO_TIMEOUT_US) {
    echoWaiting = echoRiseSeen = false;
    ready = true;
  }
  portEXIT_CRITICAL(&echoMux);
  if (ready) {
    const bool forward = pingAngle == 90 && servoAngle == 90 &&
                         uint32_t(now - servoSinceMs) >= TestConfig::SERVO_CENTER_WAIT_MS;
    sonarBlocked = !forward || duration == 0 ||
                   duration * 0.0343f / 2.0f <= TestConfig::MIN_CLEAR_CM;
    lastEchoMs = now;
  }
  if (uint32_t(now - lastEchoMs) > TestConfig::SONAR_STALE_MS) sonarBlocked = true;
  if (!echoWaiting && uint32_t(now - lastPingMs) >= TestConfig::SONAR_PERIOD_MS) {
    lastPingMs = now;
    startPing();
  }
}

void setServoAngle(int angle) {
  servoAngle = constrain(angle, 0, 180);
  const uint32_t pulse = map(servoAngle, 0, 180, TestConfig::SERVO_MIN_US, TestConfig::SERVO_MAX_US);
  ledcWrite(Config::SERVO_PINS[TestConfig::SERVO_INDEX],
            pulse * ((1UL << Config::SERVO_PWM_BITS) - 1) / (1000000UL / Config::SERVO_PWM_HZ));
}

void pollLine(bool permitted) {
  const int left = digitalRead(Config::LINE_LEFT_PIN);
  const int center = digitalRead(Config::LINE_CENTER_PIN);
  const int right = digitalRead(Config::LINE_RIGHT_PIN);
  if (activeLineLevel < 0 && left == right && center != left) activeLineLevel = center;
  if (activeLineLevel < 0) { lineLost = true; drive(0, 0); return; }
  const uint8_t pattern = (left == activeLineLevel ? 4 : 0) |
                          (center == activeLineLevel ? 2 : 0) |
                          (right == activeLineLevel ? 1 : 0);
  lineLost = pattern == 0;
  if (!permitted || lineLost) { drive(0, 0); return; }
  switch (pattern) {
    case 2: case 5: case 7: drive(TestConfig::BASE_SPEED, TestConfig::BASE_SPEED); break;
    case 4: case 6: drive(TestConfig::CORRECT_SPEED, 0); break;
    case 1: case 3: drive(0, TestConfig::CORRECT_SPEED); break;
    default: drive(0, 0); break;
  }
}
} // namespace

namespace AgvDrive {
void begin() {
  static_assert(TestConfig::DRIVER == Config::MotorDriver::L298N,
                "BTS7960 requires a separately verified pin/enable implementation");
  const uint8_t direction[] = {Config::LEFT_IN1, Config::LEFT_IN2, Config::RIGHT_IN3, Config::RIGHT_IN4};
  for (uint8_t pin : direction) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  pinMode(Config::LEFT_ENA, OUTPUT);
  pinMode(Config::RIGHT_ENB, OUTPUT);
  digitalWrite(Config::LEFT_ENA, LOW);
  digitalWrite(Config::RIGHT_ENB, LOW);
  motorPwmReady = ledcAttach(Config::LEFT_ENA, Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS) &&
                  ledcAttach(Config::RIGHT_ENB, Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS);
  if (!motorPwmReady) {
    Serial.println("[MOTOR CONFIG ERROR] PWM attach failed");
  }
  drive(0, 0);
  pinMode(Config::LINE_LEFT_PIN, INPUT);
  pinMode(Config::LINE_CENTER_PIN, INPUT);
  pinMode(Config::LINE_RIGHT_PIN, INPUT);
  pinMode(Config::ULTRASONIC_TRIG_PIN, OUTPUT);
  digitalWrite(Config::ULTRASONIC_TRIG_PIN, LOW);
  pinMode(Config::ULTRASONIC_ECHO_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(Config::ULTRASONIC_ECHO_PIN), echoChangeIsr, CHANGE);
  servoAttached = ledcAttach(Config::SERVO_PINS[TestConfig::SERVO_INDEX],
                              Config::SERVO_PWM_HZ, Config::SERVO_PWM_BITS);
  if (servoAttached) setServoAngle(90);
  servoSinceMs = lastPingMs = millis();
  lastEchoMs = 0;
}
void stop() { drive(0, 0); }
void tick(bool segmentPermitted) {
  pollSonar();
  if (!segmentPermitted || !TestConfig::ENABLE_AUTONOMOUS_MOTION || !motorPwmReady) {
    stop();
    // Preserve the supplied servo sweep as a stationary diagnostic only.
    if (servoAttached && uint32_t(millis() - servoSinceMs) >= TestConfig::SERVO_SETTLE_MS) {
      const int next = servoAngle + servoDirection * TestConfig::SERVO_STEP_DEG;
      if (next >= 180 || next <= 0) servoDirection = -servoDirection;
      setServoAngle(constrain(next, 0, 180));
      servoSinceMs = millis();
      sonarBlocked = true;
    }
    return;
  }
  if (servoAttached && servoAngle != 90) {
    setServoAngle(90);
    servoSinceMs = millis();
    sonarBlocked = true;
  }
  const bool clear = servoAttached && servoAngle == 90 &&
                     uint32_t(millis() - servoSinceMs) >= TestConfig::SERVO_CENTER_WAIT_MS &&
                     !sonarBlocked;
  pollLine(clear);
}
bool obstacle() { return sonarBlocked; }
bool lineFault() { return lineLost; }
bool motorRunning() { return running; }
}
#endif
