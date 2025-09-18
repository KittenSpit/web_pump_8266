#include "Logger.h"
#include <stdarg.h>

static void vlogWith(const __FlashStringHelper *tag, const char *fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  Serial.printf("[%s] %s\n", reinterpret_cast<const char *>(tag), buf);
}

void logInfo(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("INFO"), fmt, ap); va_end(ap); }
void logWarn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("WARN"), fmt, ap); va_end(ap); }
void logErr (const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogWith(F("ERR"),  fmt, ap); va_end(ap); }
