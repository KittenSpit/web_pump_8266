#pragma once
#include <Arduino.h>


namespace Logger {
  void begin();                               // ensure header exists (FS must be mounted)
  bool clear();                                // wipe & recreate header
  bool exists();                               // does /logs.csv exist?
  String tail(size_t maxLines);                // last N lines (text)

  void logEvent(const char* event, int pump, float runtime, float mlps,float ml, int duty, int direction, const char* status = "--");
}

void ensureHeader();
void logInfo(const char *fmt, ...);
void logWarn(const char *fmt, ...);
void logErr (const char *fmt, ...);
