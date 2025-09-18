#pragma once
#include <Arduino.h>
#include <time.h>
#include "Settings.h"

struct ScheduleFireState {
  int lastDay = -1;        // day-of-year when we last fired this index
  uint32_t lastFireMs = 0; // debounce against duplicate triggers
};

class Scheduler {
public:
  void begin();
  void loop(); // checks time-of-day and fires events

  // compute next run (seconds until next event for a pump), UINT32_MAX if none
  uint32_t nextRunSec(uint8_t pumpIdx) const;

private:
  mutable ScheduleFireState _fired[NUM_PUMPS][MAX_TIMES_PER_DAY];

  bool timeNow(struct tm &out, time_t &epoch) const;
  uint16_t secondsSinceMidnight(const struct tm &tmNow) const;
  void tryFire(uint8_t pumpIdx, uint16_t nowSec, int yday);
};

extern Scheduler scheduler;
