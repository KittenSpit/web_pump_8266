#include "PumpControl.h"
#include "Settings.h"
#include <Arduino.h>
#include "Logger.h"
#include <LittleFS.h>
// Make sure NUM_PUMPS is visible here (it should come from a shared header)
#ifndef NUM_PUMPS
#define NUM_PUMPS 3   // <-- replace with your real value if needed
#endif

// not sure it should be here
#include <time.h>

static void logEvent(const char* event, int pump, float ml, int duty, bool forward){

  ensureHeader();
  File f = LittleFS.open("/logs.csv", "a");
  if (!f) return;
  unsigned long ts = time(nullptr);  // local epoch if TZ set
  unsigned long up = millis();
  f.printf("%lu,%lu,%s,%d,%.2f,%d,%d\n",
           ts, up, event, pump, ml, duty, forward ? 1 : 0);
  f.close();
}

const char* R = "Run";
const char* P = "Prime";
const char* G = "Purge";
const char* S = "Stop";

// not sure it should be here

// ==== PWM SHIM (ESP8266 + ESP32, 0..255 duty) ====
#ifdef ARDUINO_ARCH_ESP8266
  inline void pwmSetup(uint8_t /*ch*/, uint16_t freqHz = 20000, uint16_t range = 255) {
    analogWriteFreq(freqHz);
    analogWriteRange(range);            // duty will be 0..255
  }
  inline void pwmAttachPin(uint8_t pin, uint8_t /*ch*/) { pinMode(pin, OUTPUT); }
  inline void pwmWrite(uint8_t /*ch*/, uint16_t duty, uint8_t pin) { analogWrite(pin, duty); 

  Serial.print(pin);
   Serial.print("   ");
    Serial.println(duty);
   }

#else
  inline void pwmSetup(uint8_t ch, uint32_t freqHz = 20000, uint8_t bits = 8) { ledcSetup(ch, freqHz, bits); }
  inline void pwmAttachPin(uint8_t pin, uint8_t ch) { ledcAttachPin(pin, ch); }
  inline void pwmWrite(uint8_t ch, uint32_t duty, uint8_t /*pin*/) { ledcWrite(ch, duty); }
#endif

// One-time init flags for PWM channels (file-scope, visible to both functions)
static bool s_pwmInited[NUM_PUMPS] = {};   // zero-initialized


PumpControl pumpCtl;

void PumpControl::begin(const PumpPins pins[NUM_PUMPS]) {
  for (int i = 0; i < NUM_PUMPS; ++i) {
    _pins[i] = pins[i];
    pinMode(_pins[i].pwm, OUTPUT);
    pinMode(_pins[i].dir, OUTPUT);
    writePump(i, false, false);
  }
}

void PumpControl::writePump(uint8_t idx, bool on, bool reverse) {
  if (idx >= NUM_PUMPS) return;

  // One-time PWM init for this channel/pin
  if (!s_pwmInited[idx]) {
    pinMode(_pins[idx].dir, OUTPUT);
    pwmSetup(idx, 20000 /*Hz*/, 255 /*8-bit duty range*/);
    pwmAttachPin(_pins[idx].pwm, idx);
    s_pwmInited[idx] = true;
  }

  // Duty (clamped 0..255)
  uint16_t duty = on ? settings.pump[idx].duty : 0;
  if (duty > 255) duty = 255;

  // Direction logic (same as your original)
  const bool forwardPol = settings.pump[idx].dirForward;
  const bool dirLevel   = reverse ? !forwardPol : forwardPol;
  digitalWrite(_pins[idx].dir, dirLevel ? HIGH : LOW);

  // Write PWM (shim maps to analogWrite on ESP8266, ledcWrite on ESP32)
  pwmWrite(idx, duty, _pins[idx].pwm);
}

void PumpControl::startPump(uint8_t idx, bool reverse, uint16_t seconds) {
  if (idx >= NUM_PUMPS) return;
  if (seconds == 0)     return;

  // One-time PWM setup/attach for this channel/pin
  if (!s_pwmInited[idx]) {
    pinMode(_pins[idx].dir, OUTPUT);
    pwmSetup(idx, 20000 /*Hz*/, 255 /*8-bit duty range*/);
    pwmAttachPin(_pins[idx].pwm, idx);
    s_pwmInited[idx] = true;
  }

  // Update runtime state (same as your original)
  _state[idx].running      = true;
  _state[idx].reverse      = reverse;
  _state[idx].startMs      = millis();
  _state[idx].durMs        = (uint32_t)seconds * 1000UL;
  _state[idx].deliveredML  = 0.0f;

  // Kick the pump on using your existing helper (now shimmed)
  writePump(idx, /*on=*/true, /*reverse=*/reverse);
}

void PumpControl::run(uint8_t idx, uint16_t seconds) {
   startPump(idx, false, seconds);
   float volume = seconds * settings.pump[idx].mlPerSec;
  Serial.println("Run pump "+String(idx)+" for "+String(seconds)+" seconds");
  Serial.println("ML/SEC "+String(settings.pump[idx].mlPerSec)+"  DUTY "+String(settings.pump[idx].duty)+" Target Volumne "+String(volume)+" ml");
  logEvent(R, idx, volume, settings.pump[idx].duty,  reverse);
  }

void PumpControl::prime(uint8_t idx, uint16_t seconds){ startPump(idx, false, seconds); }
void PumpControl::purge(uint8_t idx, uint16_t seconds){ startPump(idx, true , seconds); }

void PumpControl::stop(uint8_t idx) {
  if (idx >= NUM_PUMPS) return;
  writePump(idx, false, false);
  float runTime =((millis() - _state[idx].startMs)/1000);
  float deliveredML = roundf(_state[idx].deliveredML * 10.0f) / 10.0f;
  Serial.println("Stop pump "+String(idx)+" after "+String(runTime)+" seconds");
  Serial.println("ML/SEC "+String(settings.pump[idx].mlPerSec)+"  DUTY "+String(settings.pump[idx].duty)+ "  Delivered Volumne "+String(deliveredML)+" ml");
  logEvent(S, idx, deliveredML, settings.pump[idx].duty,  reverse);
  _state[idx].running = false;
  _state[idx].durMs = 0;
}

bool PumpControl::isRunning(uint8_t idx) const {
  return (idx < NUM_PUMPS) ? _state[idx].running : false;
}

void PumpControl::loop() {
  uint32_t now = millis();
  for (int i = 0; i < NUM_PUMPS; ++i) {
    if (!_state[i].running) continue;
    uint32_t elapsed = now - _state[i].startMs;
    // integrate delivered ml
    _state[i].deliveredML = (elapsed / 1000.0f) * settings.pump[i].mlPerSec;
    if (elapsed >= _state[i].durMs) {
     // Serial.println("elapsed "+String(elapsed)+"  durMs "+String(_state[i].durMs));  
      stop(i);
    }
  }
}


