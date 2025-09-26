#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

// Adjust to your actual path
#ifndef LOG_FILE_PATH
//#define LOG_FILE_PATH "/logs/doser.csv"
#define LOG_FILE_PATH "/logs.csv"
#endif

// --- Small helpers ----
inline String jsonEscape(const String& s) {
  String out; out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) { // control char
          char buf[7]; // \u00XX
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

inline int indexOfIgnoreCase(const String& hay, const String& needle) {
  String H = hay; H.toLowerCase();
  String N = needle; N.toLowerCase();
  return H.indexOf(N);
}

// Split CSV line (comma or semicolon) with simple quote handling.
// Good enough for your format (no embedded newlines).
static void splitCsvLine(const String& line, char delim, std::vector<String>& out) {
  out.clear();
  String cur; cur.reserve(line.length());
  bool inQuotes = false;
  for (size_t i = 0; i < line.length(); ++i) {
    char ch = line[i];
    if (ch == '\"') {
      // toggle quotes (basic CSV; duplicated quotes not handled)
      inQuotes = !inQuotes;
    } else if (!inQuotes && ch == delim) {
      out.push_back(cur);
      cur = String();
    } else {
      cur += ch;
    }
  }
  out.push_back(cur);
}

// Trim \r and trailing spaces
inline String rstrip(const String& s) {
  int end = s.length() - 1;
  while (end >= 0 && (s[end] == '\r' || s[end] == ' ' || s[end] == '\t')) end--;
  return s.substring(0, end + 1);
}

// Detect delimiter from header line
inline char detectDelim(const String& header) {
  int c = 0, sc = 0;
  for (size_t i = 0; i < header.length(); ++i) {
    if (header[i] == ',') c++;
    if (header[i] == ';') sc++;
  }
  return (sc > c) ? ';' : ',';
}

// Stream the entire file into a response (raw)
static void streamFileToResponse(File& f, AsyncResponseStream* res) {
  uint8_t buf[1024];
  while (true) {
    size_t n = f.read(buf, sizeof(buf));
    if (!n) break;
    res->write(buf, n);
  }
}

// --- Route installers ----
static void installLogRoutes(
  AsyncWebServer& server,
  const char* pathJson = "/api/log.json",
  const char* pathCsv  = "/api/log.csv",
  const char* pathClear= "/api/log_clear",
  const char* pathList = "/api/log_list"   // optional passthrough of raw lines
) {
  // JSON endpoint
  server.on(pathJson, HTTP_GET, [](AsyncWebServerRequest* request){
    if (!LittleFS.exists(LOG_FILE_PATH)) {
      auto* res = request->beginResponseStream("application/json");
      res->addHeader("Cache-Control","no-store");
      res->print("[]");
      request->send(res);
      return;
    }
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) {
      request->send(500, "application/json", "{\"error\":\"log open failed\"}");
      return;
    }

    auto* res = request->beginResponseStream("application/json; charset=utf-8");
    res->addHeader("Cache-Control","no-store");
    // CORS (optional)
    res->addHeader("Access-Control-Allow-Origin", "*");

    // Read first line = header
    String header = rstrip(f.readStringUntil('\n'));
    if (header.length() == 0) {
      res->print("[]");
      request->send(res);
      f.close();
      return;
    }

    char delim = detectDelim(header);
    std::vector<String> cols;
    splitCsvLine(header, delim, cols);

    // Map column names -> index (case-insensitive)
    int idx_ts = -1, idx_uptime=-1, idx_event=-1, idx_pump=-1, idx_runtime=-1,
        idx_mlps=-1, idx_ml=-1, idx_duty=-1, idx_dir=-1, idx_status=-1;

    for (size_t i=0;i<cols.size();++i) {
      String c = cols[i]; c.trim();
      if (indexOfIgnoreCase(c, "ts")        == 0) idx_ts = i;
      else if (indexOfIgnoreCase(c, "uptime_ms") == 0) idx_uptime = i;
      else if (indexOfIgnoreCase(c, "event")    == 0) idx_event = i;
      else if (indexOfIgnoreCase(c, "pump")     == 0) idx_pump = i;
      else if (indexOfIgnoreCase(c, "runtime")  == 0) idx_runtime = i;
      else if (indexOfIgnoreCase(c, "mlps")     == 0) idx_mlps = i;
      else if (indexOfIgnoreCase(c, "ml")       == 0 && idx_ml < 0) idx_ml = i; // first ml
      else if (indexOfIgnoreCase(c, "duty")     == 0) idx_duty = i;
      else if (indexOfIgnoreCase(c, "dir")      == 0) idx_dir = i;
      else if (indexOfIgnoreCase(c, "status")      == 0) idx_status = i;
    }

    // Stream JSON array
    res->print("[\n");
    bool first = true;

    std::vector<String> parts;
    while (f.available()) {
      String line = rstrip(f.readStringUntil('\n'));
      if (line.length() == 0) continue; // skip blanks
      splitCsvLine(line, delim, parts);

      // Safe access
      auto getS = [&](int i)->String { return (i >= 0 && (size_t)i < parts.size()) ? parts[i] : String(); };
      auto getD = [&](int i)->double { return (i >= 0 && (size_t)i < parts.size()) ? parts[i].toDouble() : 0.0; };
      auto getI = [&](int i)->long   { return (i >= 0 && (size_t)i < parts.size()) ? parts[i].toInt() : 0; };

      String ts    = getS(idx_ts);     // e.g. "2025-09-20 00:18:13"
      long uptime  = (long)getD(idx_uptime);
      String ev    = getS(idx_event);
      int pump     = (int)getI(idx_pump);
      double run_s = getD(idx_runtime);
      double mlps  = getD(idx_mlps);
      double ml    = getD(idx_ml);
      int duty     = (int)getI(idx_duty);
      int dir      = (int)getI(idx_dir);
      String status    = getS(idx_status);

      // Optional: normalize ts to ISO by replacing ' ' with 'T' (commented out)
      // ts.replace(' ', 'T'); // and optionally append 'Z'

      if (!first) res->print(",\n");
      first = false;

      res->print("  {");
      // Strings escaped; numbers plain
      res->printf("\"ts\":\"%s\"", jsonEscape(ts).c_str());
      res->printf(",\"uptime_ms\":%ld", uptime);
      res->printf(",\"event\":\"%s\"", jsonEscape(ev).c_str());
      res->printf(",\"pump\":%d", pump);
      res->printf(",\"runtime\":%.3f", run_s);
      res->printf(",\"mlps\":%.3f", mlps);
      res->printf(",\"ml\":%.3f", ml);
      res->printf(",\"duty\":%d", duty);
      res->printf(",\"dir\":%d", dir);
      res->printf(",\"status\":\"%s\"", jsonEscape(status).c_str());
      res->print("}");
    }
    res->print("\n]\n");

    request->send(res);
    f.close();
  });

  // CSV download passthrough
  server.on(pathCsv, HTTP_GET, [](AsyncWebServerRequest* request){
    if (!LittleFS.exists(LOG_FILE_PATH)) {
      request->send(404, "text/plain", "No log");
      return;
    }
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) { request->send(500, "text/plain", "log open failed"); return; }
    auto* res = request->beginResponseStream("text/csv; charset=utf-8");
    res->addHeader("Cache-Control","no-store");
    res->addHeader("Content-Disposition","attachment; filename=\"doser_log.csv\"");
    streamFileToResponse(f, res);
    request->send(res);
    f.close();
  });

  // Clear (truncate) log
  server.on(pathClear, HTTP_POST, [](AsyncWebServerRequest* request){
    // Truncate by re-creating the file
    File f = LittleFS.open(LOG_FILE_PATH, "w");
    if (!f) { request->send(500, "application/json", "{\"ok\":false,\"error\":\"open fail\"}"); return; }
    f.close();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // Optional: raw list passthrough (screen style)
  server.on(pathList, HTTP_GET, [](AsyncWebServerRequest* request){
    if (!LittleFS.exists(LOG_FILE_PATH)) { request->send(404, "text/plain", "No log"); return; }
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) { request->send(500, "text/plain", "log open failed"); return; }
    auto* res = request->beginResponseStream("text/plain; charset=utf-8");
    res->addHeader("Cache-Control","no-store");
    streamFileToResponse(f, res);
    request->send(res);
    f.close();
  });
}

