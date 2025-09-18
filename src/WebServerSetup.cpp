#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include "lwip_enum_fix.h"     // <-- between WiFi and AsyncWebServer
#include <ESPAsyncWebServer.h>
#include <Updater.h>
#include <ArduinoJson.h>

#include <LittleFS.h>

#include "WebServerSetup.h"
#include "Settings.h"
#include "PumpControl.h"
#include "Scheduler.h"

#include "WebServerSetup.h"
#include "Logger.h"
// Adjust as you like
static AsyncWebServer server(80);


//AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static String statusJson() {
  JsonDocument doc;
  doc["uptime_ms"] = millis();

  JsonArray parr = doc["pumps"].to<JsonArray>();
  for (int i = 0; i < NUM_PUMPS; ++i) {
    const auto &s = pumpCtl.state(i);
    JsonObject o = parr.add<JsonObject>();
    o["idx"] = i;
    o["running"] = s.running;
    o["reverse"] = s.reverse;
    o["start_ms"] = s.startMs;
    o["dur_ms"] = s.durMs;
    o["delivered_ml"] = s.deliveredML;
    o["ml_per_sec"] = settings.pump[i].mlPerSec;
    o["duty"] = settings.pump[i].duty;
    uint32_t due = scheduler.nextRunSec(i);
    o["next_run_s"] = (due == UINT32_MAX) ? -1 : (int32_t)due;
  }

  String out; serializeJson(doc, out);
  return out;
}

static void wsBroadcastStatus() {
  String s = statusJson();
  ws.textAll(s);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(statusJson());
  } else if (type == WS_EVT_DATA) {
    // optional: parse small commands if you want
  }
}

void webserverBegin() {
    Serial.println(F("Mounting LittleFS..."));
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed!"));
  }
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Static files from LittleFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", statusJson());
  });

  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", settingsToJson());
  });

/*   server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      String body((const char*)data, len);
      String err;
      if (settingsFromJson(body, err) && settingsSave()) {
        req->send(200, "application/json", "{\"ok\":true}");
        wsBroadcastStatus();
      } else {
        String m = String("{\"ok\":false,\"err\":\"") + err + "\"}";
        req->send(400, "application/json", m);
      }
    }); */

/* server.on("/api/settings", HTTP_POST,
  // Done callback
  [](AsyncWebServerRequest* req) {
    // Processed in the upload callback; just reply here.
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  },
  // Upload (body) callback: called for each chunk
  [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t* data, size_t len, bool final) {
    // Allocate a per-request String on first chunk
    if (index == 0) {
      auto *buf = new String();
      buf->reserve(req->contentLength());   // reserve total size if known
      req->_tempObject = buf;
    }

    // Append this chunk
    auto *buf = static_cast<String*>(req->_tempObject);
    buf->concat((const char*)data, len);

    // Last chunk: parse & apply
    if (final) {
      // buf->c_str() holds full body
      // TODO: parse JSON here (ArduinoJson)
      // StaticJsonDocument<1024> doc; deserializeJson(doc, *buf); ...

      // Clean up
      delete buf;
      req->_tempObject = nullptr;
    }
  }
); */


server.on("/api/settings", HTTP_POST,
  // onRequest: we’ll send the response from the body handler; keep this empty.
  [](AsyncWebServerRequest* req){},
  // onUpload (file upload): not used
  nullptr,
  // onBody: called for each chunk of the request body
  [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
    // First chunk: allocate a per-request buffer
    if (index == 0) {
      auto* buf = new String();
      if (total > 0) buf->reserve(total);     // reserve if Content-Length is known
      req->_tempObject = buf;                 // stash it on the request
    }

    // Append this chunk
    auto* buf = static_cast<String*>(req->_tempObject);
    buf->concat(reinterpret_cast<const char*>(data), len);

    // Last chunk? Parse, apply, save, respond, clean up
    if (index + len == total) {
      String err;
      bool ok = settingsFromJson(*buf, err) && settingsSave();

      // cleanup buffer before responding
      delete buf;
      req->_tempObject = nullptr;

      if (ok) {
        wsBroadcastStatus();  // if you want clients to refresh
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        String m = String("{\"ok\":false,\"err\":\"") + err + "\"}";
        req->send(400, "application/json", m);
      }
    }
  }
);


  // Controls
  server.on("/api/run", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      JsonDocument doc; deserializeJson(doc, data, len);
      uint8_t idx = doc["idx"] | 0;
      uint16_t sec = doc["sec"] | settings.pump[idx].defaultRunSec;
      pumpCtl.run(idx, sec);
      req->send(200, "application/json", "{\"ok\":true}");
      wsBroadcastStatus();
    });

  server.on("/api/prime", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      JsonDocument doc; deserializeJson(doc, data, len);
      uint8_t idx = doc["idx"] | 0;
      uint16_t sec = doc["sec"] | 3;
      pumpCtl.prime(idx, sec);
      req->send(200, "application/json", "{\"ok\":true}");
      wsBroadcastStatus();
    });

  server.on("/api/purge", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      JsonDocument doc; deserializeJson(doc, data, len);
      uint8_t idx = doc["idx"] | 0;
      uint16_t sec = doc["sec"] | 2;
      pumpCtl.purge(idx, sec);
      req->send(200, "application/json", "{\"ok\":true}");
      wsBroadcastStatus();
    });

  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      JsonDocument doc; deserializeJson(doc, data, len);
      uint8_t idx = doc["idx"] | 0;
      pumpCtl.stop(idx);
      req->send(200, "application/json", "{\"ok\":true}");
      wsBroadcastStatus();
    });

  // OTA
 // AsyncElegantOTA.begin(&server);

  server.begin();
  logInfo("HTTP server started");
}

void webserverLoop() {
  static uint32_t lastPush = 0;
  if (millis() - lastPush > 1000) {
    lastPush = millis();
    wsBroadcastStatus();
  }
  ws.cleanupClients();
}
