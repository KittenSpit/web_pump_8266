#pragma once
#include <Arduino.h>
#include "Settings.h"

// Map each pump to DRV8871 pins (PWM + DIR). Adjust to your wiring.
struct PumpPins {
  uint8_t pwm;
  uint8_t dir;
};

struct PumpRuntime {
  bool running = false;
  bool reverse = false;   // for purge
  uint32_t startMs = 0;
  uint32_t durMs = 0;
  float deliveredML = 0.0f;
};

class PumpControl {
public:
  void begin(const PumpPins pins[NUM_PUMPS]);
  void loop(); // call in main loop

  void run(uint8_t idx, uint16_t seconds);
  void prime(uint8_t idx, uint16_t seconds);
  void purge(uint8_t idx, uint16_t seconds);

  void stop(uint8_t idx);
  bool isRunning(uint8_t idx) const;
  const PumpRuntime& state(uint8_t idx) const { return _state[idx]; }

private:
  PumpPins _pins[NUM_PUMPS];
  PumpRuntime _state[NUM_PUMPS];

  void startPump(uint8_t idx, bool reverse, uint16_t seconds);
  void writePump(uint8_t idx, bool on, bool reverse);
};

extern PumpControl pumpCtl;
