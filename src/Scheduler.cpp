#include "Scheduler.h"
#include "PumpControl.h"

Scheduler scheduler;

void Scheduler::begin() {
  // nothing
}

bool Scheduler::timeNow(struct tm &out, time_t &epoch) const {
  epoch = time(nullptr);
  if (epoch < 100000) return false; // not synced yet
  localtime_r(&epoch, &out);
  return true;
}

uint16_t Scheduler::secondsSinceMidnight(const struct tm &tmNow) const {
  return uint16_t(tmNow.tm_hour * 3600 + tmNow.tm_min * 60 + tmNow.tm_sec);
}

void Scheduler::tryFire(uint8_t i, uint16_t nowSec, int yday) {
  const PumpConfig &pc = settings.pump[i];
  for (int t = 0; t < pc.timesCount; ++t) {
    uint16_t due = pc.timesSec[t];
    // fire when we cross the boundary: within this 1-second tick window
    if (nowSec == due) {
      auto &fs = _fired[i][t];
      // prevent duplicate firing the same day or within 3s
      if (fs.lastDay == yday && (millis() - fs.lastFireMs) < 3000) continue;

      // seconds needed to deliver doseML at mlPerSec
      float ml = pc.doseML[t];
      float mls = max(0.01f, pc.mlPerSec);
      uint16_t needSec = uint16_t(ceilf(ml / mls));
      if (needSec > 0) {
        pumpCtl.run(i, needSec);
      }
      fs.lastDay = yday;
      fs.lastFireMs = millis();
    }
  }
}

void Scheduler::loop() {
  struct tm tmNow;
  time_t epoch;
  if (!timeNow(tmNow, epoch)) return;

  uint16_t nowSec = secondsSinceMidnight(tmNow);
  for (uint8_t i = 0; i < NUM_PUMPS; ++i) {
    tryFire(i, nowSec, tmNow.tm_yday);
  }
}

uint32_t Scheduler::nextRunSec(uint8_t pumpIdx) const {
  struct tm tmNow;
  time_t epoch;
  if (!timeNow(tmNow, epoch)) return UINT32_MAX;
  uint16_t nowSec = secondsSinceMidnight(tmNow);

  const PumpConfig &pc = settings.pump[pumpIdx];
  uint32_t best = UINT32_MAX;

  // Today forward
  for (int t = 0; t < pc.timesCount; ++t) {
    uint16_t due = pc.timesSec[t];
    if (due >= nowSec) {
      best = min<uint32_t>(best, due - nowSec);
    }
  }
  // If none left today, pick the earliest tomorrow
  if (best == UINT32_MAX && pc.timesCount > 0) {
    uint16_t earliest = 65535;
    for (int t = 0; t < pc.timesCount; ++t) earliest = min<uint16_t>(earliest, pc.timesSec[t]);
    best = 24UL*3600UL - nowSec + earliest;
  }
  return best;
}
