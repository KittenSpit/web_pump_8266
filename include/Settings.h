#pragma once
#include <ArduinoJson.h>

constexpr uint8_t NUM_PUMPS = 3;         // adjust for your build
constexpr uint8_t MAX_TIMES_PER_DAY = 8; // up to 8 dose times per pump

struct PumpConfig {
  float mlPerSec = 1.0f;   // calibration: flow rate
  uint8_t duty = 200;      // 0..255
  uint16_t defaultRunSec = 5; // used for manual Run button
  uint8_t dirForward = 1;  // forward polarity (0/1) in case motor wired reversed
  // daily schedule
  uint8_t timesCount = 0;
  uint16_t timesSec[MAX_TIMES_PER_DAY] = {}; // seconds since midnight
  float doseML[MAX_TIMES_PER_DAY] = {};      // ml at each time
};


struct Settings {
  char wifiSsid[32]   = "PHD1 2.4";
  char wifiPass[64]   = "Andrew1Laura2";
  char hostname[32]   = "Esp32-doser";
  PumpConfig pump[NUM_PUMPS];
  uint16_t tzOffsetMinutes =  -240; // EDT default, will be adjusted via TZ string anyway
  bool useDST = true;
};

extern Settings settings;

bool settingsLoad();
bool settingsSave();
String settingsToJson();           // for GET /api/settings
bool settingsFromJson(const String &body, String &err); // for POST /api/settings
