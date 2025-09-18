#include <Arduino.h>
#include <time.h>
#include "Settings.h"   // uses settings.useDST

// --- Eastern Time base offsets (Toronto) ---
static const long ET_STD_OFFSET = -5 * 3600; // UTC-5 (EST)
static const long ET_DST_OFFSET =  1 * 3600; // add during DST

// Sakamoto's algorithm: weekday (0=Sun .. 6=Sat)
static int weekdayYMD(int y, int m, int d) {
  static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) y -= 1;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

// Nth weekday in month (e.g., 2nd Sunday in March)
static int nthWeekdayOfMonth(int year, int month, int weekday /*0=Sun..6=Sat*/, int nth /*1..5*/) {
  int w_first = weekdayYMD(year, month, 1);
  int delta = (weekday - w_first + 7) % 7;
  return 1 + delta + (nth - 1) * 7;
}

// North American DST for Eastern Time
static bool isDST_Eastern_NA(int year, int mon /*1-12*/, int mday, int hour /*0-23*/) {
  if (mon < 3 || mon > 11) return false;      // Jan/Feb, Dec => standard
  if (mon > 3 && mon < 11) return true;       // Apr..Oct => DST

  if (mon == 3) {                              // starts 2nd Sunday in March @02:00
    int startDay = nthWeekdayOfMonth(year, 3, 0, 2);
    if (mday > startDay) return true;
    if (mday < startDay) return false;
    return hour >= 2;
  } else {                                     // mon == 11, ends 1st Sunday @02:00
    int endDay = nthWeekdayOfMonth(year, 11, 0, 1);
    if (mday < endDay) return true;
    if (mday > endDay) return false;
    return hour < 2;
  }
}

// Wait for an SNTP time that’s clearly valid
static bool waitForTimeSync(uint32_t timeout_ms = 8000) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    time_t now = time(nullptr);
    if (now > 1700000000) return true; // definitely synced (≈2023+)
    delay(200);
  }
  return false;
}

static bool getLocalTm(struct tm& out) {
  time_t now = time(nullptr);
  if (now <= 0) return false;
  localtime_r(&now, &out);
  return true;
}

// Public helpers you can call elsewhere
uint32_t secondsSinceMidnight() {
  struct tm t{};
  if (!getLocalTm(t)) return 0;
  return (uint32_t)t.tm_hour * 3600u + (uint32_t)t.tm_min * 60u + (uint32_t)t.tm_sec;
}

void printCurrentTimeInfo(const char* tag = "TIME") {
  time_t now = time(nullptr);
  struct tm utc{}, loc{};
  gmtime_r(&now, &utc);
  localtime_r(&now, &loc);

  // crude abbrev based on DST status
  bool inDST = false;
  {
    int y = loc.tm_year + 1900, m = loc.tm_mon + 1;
    inDST = isDST_Eastern_NA(y, m, loc.tm_mday, loc.tm_hour);
  }

  Serial.printf("[%s] UTC  : %04d-%02d-%02d %02d:%02d:%02d\n",
    tag, utc.tm_year+1900, utc.tm_mon+1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
  Serial.printf("[%s] LOCAL: %04d-%02d-%02d %02d:%02d:%02d  (%s)\n",
    tag, loc.tm_year+1900, loc.tm_mon+1, loc.tm_mday, loc.tm_hour, loc.tm_min, loc.tm_sec,
    inDST ? "EDT" : "EST");
}

// --- MAIN ENTRYPOINT you call after Wi-Fi connects ---
// Behavior:
//   - If settings.useDST == false  -> lock to EST (UTC-5, no DST)
//   - If settings.useDST == true   -> auto-DST (compute now, apply +1h if in DST)
void startTime_AutoDST_Toronto() {
  // Step 1: start at standard time (EST)
  configTime(ET_STD_OFFSET, 0, "pool.ntp.org", "time.nist.gov");
  waitForTimeSync();

  // If user disabled DST in settings, keep standard time and return
  if (!settings.useDST) {
    printCurrentTimeInfo("TIME(EST-only)");
    return;
  }

  // Step 2: compute if we are in DST right now and re-apply offsets
  struct tm lt{};
  if (!getLocalTm(lt)) {
    printCurrentTimeInfo("TIME(sync-pending)");
    return;
  }
  int year = lt.tm_year + 1900;
  int mon  = lt.tm_mon + 1;
  bool inDST = isDST_Eastern_NA(year, mon, lt.tm_mday, lt.tm_hour);

  configTime(ET_STD_OFFSET, inDST ? ET_DST_OFFSET : 0, "pool.ntp.org", "time.nist.gov");
  delay(500);
  printCurrentTimeInfo(inDST ? "TIME(EDT)" : "TIME(EST)");
}
