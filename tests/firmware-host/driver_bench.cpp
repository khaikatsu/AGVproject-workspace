#include <cassert>
#include "Arduino.h"
#include "../../firmware/AGV_Connection_Test/driver.cpp"

static void advance(uint32_t ms) { hostMillis=ms; hostMicros=ms*1000; }
static void echo(uint32_t durationUs) {
  hostPins[Config::ULTRASONIC_ECHO_PIN]=HIGH;
  hostMicros+=100;
  echoChangeIsr();
  hostPins[Config::ULTRASONIC_ECHO_PIN]=LOW;
  hostMicros+=durationUs;
  echoChangeIsr();
}

int main() {
  hostPins[Config::LINE_LEFT_PIN]=LOW;
  hostPins[Config::LINE_CENTER_PIN]=HIGH;
  hostPins[Config::LINE_RIGHT_PIN]=LOW;
  AgvDrive::begin();
  assert(!AgvDrive::motorRunning());
  advance(1000);AgvDrive::tick(true); // no forward sonar sample yet
  assert(!AgvDrive::motorRunning());
  echo(3000);advance(1001);AgvDrive::tick(true); // about 51 cm, centered line
  assert(!AgvDrive::obstacle() && !AgvDrive::lineFault() && AgvDrive::motorRunning());
  hostPins[Config::LINE_LEFT_PIN]=HIGH;
  hostPins[Config::LINE_CENTER_PIN]=LOW;
  hostPins[Config::LINE_RIGHT_PIN]=HIGH;
  AgvDrive::tick(true); // 101 is also a straight-line reading
  assert(!AgvDrive::lineFault() && AgvDrive::motorRunning());
  hostPins[Config::LINE_LEFT_PIN]=LOW;
  hostPins[Config::LINE_CENTER_PIN]=HIGH;
  hostPins[Config::LINE_RIGHT_PIN]=LOW;
  AgvDrive::tick(false);assert(!AgvDrive::motorRunning()); // server STOP / lost link
  advance(2000);AgvDrive::tick(true); // center the diagnostic sweep first
  advance(3000);AgvDrive::tick(true);echo(3000);advance(3001);AgvDrive::tick(true);
  assert(AgvDrive::motorRunning());
  hostPins[Config::LINE_CENTER_PIN]=LOW;
  AgvDrive::tick(true);
  assert(AgvDrive::lineFault() && !AgvDrive::motorRunning());
  hostPins[Config::LINE_CENTER_PIN]=HIGH;
  advance(4000);AgvDrive::tick(true);echo(870);advance(4001);AgvDrive::tick(true);
  assert(AgvDrive::obstacle() && !AgvDrive::motorRunning()); // ~14.9 cm
  return 0;
}
