#include "Logger.h"
#include <stdarg.h>

#include <LittleFS.h>
#include <FS.h>
#include <time.h>

namespace {
  const char* kLogPath = "/logs.csv";


}
  void ensureHeader() {
    if (LittleFS.exists(kLogPath)) return;
    File f = LittleFS.open(kLogPath, "w");
    if (!f) return;
    f.println(F("ts,uptime_ms,event,pump,ml,duty,dir"));
    f.close();
  }
  
// FS must already be mounted elsewhere
void Logger::begin() { ensureHeader(); }

bool Logger::clear() {
  LittleFS.remove(kLogPath);
  ensureHeader();
  return true;
}

bool Logger::exists() { return LittleFS.exists(kLogPath); }

// Efficient tail N lines
String Logger::tail(size_t maxLines) {
  File f = LittleFS.open(kLogPath, "r");
  if (!f) return String();
  int64_t pos = (int64_t)f.size() - 1;
  size_t lines = 0;
  while (pos >= 0 && lines <= maxLines) {
    f.seek(pos, SeekSet);
    int c = f.read();
    if (c == '\n') { lines++; if (lines > maxLines) { pos++; break; } }
    pos--;
  }
  if (pos < 0) pos = 0;
  f.seek(pos, SeekSet);
  String out; out.reserve(4096);
  while (f.available()) out += char(f.read());
  f.close();
  return out;
}

void Logger::logEvent(const char* event, int pump, float ml, int duty, bool forward) {
  ensureHeader();
  File f = LittleFS.open(kLogPath, "a");
  if (!f) return;
  const unsigned long ts = time(nullptr);
  const unsigned long up = millis();
  f.printf("%lu,%lu,%s,%d,%.2f,%d,%d\n",
           ts, up, event, pump, ml, duty, forward ? 1 : 0);
  f.close();
}



static void vlogWith(const __FlashStringHelper *tag, const char *fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  Serial.printf("[%s] %s\n", reinterpret_cast<const char *>(tag), buf);
}

void logInfo(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("INFO"), fmt, ap); va_end(ap); }
void logWarn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("WARN"), fmt, ap); va_end(ap); }
void logErr (const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("ERR"),  fmt, ap); va_end(ap); }
