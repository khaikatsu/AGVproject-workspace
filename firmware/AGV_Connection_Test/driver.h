#pragma once

// Physical control is a local safety layer. MQTT callbacks never drive motors.
namespace AgvDrive {
void begin();
void stop();
void tick(bool segmentPermitted);
bool obstacle();
bool lineFault();
bool motorRunning();
}
