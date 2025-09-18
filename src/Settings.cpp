#include "Settings.h"
#include <LittleFS.h>

Settings settings; // global

static const char *kSettingsPath = "/settings.json";

bool settingsLoad() {
  if (!LittleFS.exists(kSettingsPath)) return settingsSave(); // write defaults
  File f = LittleFS.open(kSettingsPath, "r");
  if (!f) return false;
  String s = f.readString();
  f.close();

  String err;
  if (!settingsFromJson(s, err)) {
    // if corrupted, keep defaults and save back
    return settingsSave();
  }
  return true;
}

bool settingsSave() {
  File f = LittleFS.open(kSettingsPath, "w");
  if (!f) return false;
  String s = settingsToJson();
  f.print(s);
  Serial.println("Settings saved: " + s);
  f.close();
  return true;
}

String settingsToJson() {
  JsonDocument doc;

  doc["wifi"]["ssid"] = settings.wifiSsid;
  doc["wifi"]["pass"] = settings.wifiPass;
  doc["hostname"] = settings.hostname;
  doc["tzOffsetMinutes"] = settings.tzOffsetMinutes;
  doc["useDST"] = settings.useDST;

  JsonArray arr = doc["pumps"].to<JsonArray>();
  for (int i = 0; i < NUM_PUMPS; ++i) {
    JsonObject p = arr.add<JsonObject>();
    p["idx"] = i;
    p["mlPerSec"] = settings.pump[i].mlPerSec;
    p["duty"] = settings.pump[i].duty;
    p["defaultRunSec"] = settings.pump[i].defaultRunSec;
    p["dirForward"] = settings.pump[i].dirForward;

    JsonArray times = p["times"].to<JsonArray>();
    for (int t = 0; t < settings.pump[i].timesCount; ++t) {
      JsonObject o = times.add<JsonObject>();
      o["sec"] = settings.pump[i].timesSec[t];
      o["ml"] = settings.pump[i].doseML[t];
    }
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// helper to clamp
static uint8_t clamp_u8(uint32_t v) { return (v > 255) ? 255 : (uint8_t)v; }
static uint16_t clamp_u16(uint32_t v){ return (v > 65535) ? 65535 : (uint16_t)v; }

bool settingsFromJson(const String &body, String &err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) { err = e.c_str(); return false; }

  if (doc["wifi"]["ssid"].is<const char*>()) strlcpy(settings.wifiSsid, doc["wifi"]["ssid"]|"", sizeof(settings.wifiSsid));
  if (doc["wifi"]["pass"].is<const char*>()) strlcpy(settings.wifiPass, doc["wifi"]["pass"]|"", sizeof(settings.wifiPass));
  if (doc["hostname"].is<const char*>())     strlcpy(settings.hostname, doc["hostname"]|"", sizeof(settings.hostname));
  settings.tzOffsetMinutes = doc["tzOffsetMinutes"] | settings.tzOffsetMinutes;
  settings.useDST = doc["useDST"] | settings.useDST;

  if (doc["pumps"].is<JsonArray>()) {
    JsonArray arr = doc["pumps"].as<JsonArray>();
    for (JsonObject p : arr) {
      int idx = p["idx"] | -1;
      if (idx < 0 || idx >= NUM_PUMPS) continue;
      settings.pump[idx].mlPerSec = p["mlPerSec"] | settings.pump[idx].mlPerSec;
      settings.pump[idx].duty = clamp_u8(p["duty"] | settings.pump[idx].duty);
      settings.pump[idx].defaultRunSec = clamp_u16(p["defaultRunSec"] | settings.pump[idx].defaultRunSec);
      settings.pump[idx].dirForward = clamp_u8(p["dirForward"] | settings.pump[idx].dirForward) ? 1 : 0;

      // times
      settings.pump[idx].timesCount = 0;
      if (p["times"].is<JsonArray>()) {
        for (JsonObject o : p["times"].as<JsonArray>()) {
          if (settings.pump[idx].timesCount >= MAX_TIMES_PER_DAY) break;
          uint16_t sec = o["sec"] | 0;
          float ml     = o["ml"]  | 0.0f;
          settings.pump[idx].timesSec[settings.pump[idx].timesCount] = sec;
          settings.pump[idx].doseML[settings.pump[idx].timesCount]   = ml;
          settings.pump[idx].timesCount++;
        }
      }
    }
  }
  return true;
}
