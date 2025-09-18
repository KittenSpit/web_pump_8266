#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ArduinoJson.h>
#include "lwip_enum_fix.h"               // <-- our shim sits between WiFi and AsyncWebServer
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <time.h>
#include "TimeSetup.h"
//include <ElegantOTA.h> 

#include "Settings.h"
#include "PumpControl.h"
#include "Scheduler.h"
#include "WebServerSetup.h"
#include "Logger.h"

// ---- Pin map (edit these) ----
// Example pins for ESP32 DevKit + DRV8871
static const PumpPins PINS[NUM_PUMPS] = {
  { .pwm = 12, .dir = 13 }, // Pump 0
  { .pwm = 27, .dir = 14 }, // Pump 1
  { .pwm = 12, .dir = 13 }, // Pump 2
};

// America/Toronto POSIX TZ (handles DST); ESP32 uses this with localtime_r.
//static const char *TZ_TORONTO = "EST5EDT,M3.2.0/2,M11.1.0/2";

  char wifiSsid[32]   = "PHD1 2.4";
  char wifiPass[64]   = "Andrew1Laura2";
  char hostname[32]   = "esp32-doser";




static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(settings.hostname);
  WiFi.begin(settings.wifiSsid, settings.wifiPass);
  logInfo("Connecting WiFi SSID=%s ...", settings.wifiSsid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    logInfo("WiFi OK: %s  IP=%s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    logWarn("WiFi connect failed, starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(settings.hostname);
    logInfo("AP SSID: %s  IP=%s", settings.hostname, WiFi.softAPIP().toString().c_str());
  }
}



/* static void startTime() {
  // Toronto: UTC-5 standard, +3600 for DST (summer)
  long gmtOffset = -5 * 3600;
  long dstOffset = settings.useDST ? 3600 : 0;
  configTime(gmtOffset, dstOffset, "pool.ntp.org", "time.nist.gov");
  delay(5000); // wait a bit for time to be set
  time_t now = time(nullptr);
struct tm t;
localtime_r(&now, &t);

int secToday = t.tm_hour*3600 + t.tm_min*60 + t.tm_sec;
Serial.printf("Time now: %02d:%02d:%02d (seconds=%d)\n",t.tm_hour, t.tm_min, t.tm_sec, secToday);
} */

void setup() {
  Serial.begin(115200);
  delay(100);

  if (!LittleFS.begin()) {
    logErr("LittleFS mount failed");
  }

  settingsLoad();

  connectWiFi();
  //startTime();
  startTime_AutoDST_Toronto();
  printCurrentTimeInfo();
  Serial.printf("secSinceMidnight = %u\n", secondsSinceMidnight());


  pumpCtl.begin(PINS);
  scheduler.begin();
  webserverBegin();
}

void loop() {
  pumpCtl.loop();
  scheduler.loop();
  webserverLoop();
  delay(10);
}
