#pragma once
#include <Arduino.h>


namespace Logger {
  void begin();                               // ensure header exists (FS must be mounted)
  bool clear();                                // wipe & recreate header
  bool exists();                               // does /logs.csv exist?
  String tail(size_t maxLines);                // last N lines (text)
  void logEvent(const char* event, int pump, float ml, int duty, bool forward);
}
void ensureHeader();
void logInfo(const char *fmt, ...);
void logWarn(const char *fmt, ...);
void logErr (const char *fmt, ...);
