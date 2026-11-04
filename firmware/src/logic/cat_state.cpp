#include "cat_state.h"
#include "../config.h"

static void setMood(CatState& c, CatMood m, uint32_t nowMs) {
  c.mood = m;
  c.since = nowMs;
}

// wrap-safe "now has reached deadline"
static bool reached(uint32_t nowMs, uint32_t deadline) {
  return (int32_t)(nowMs - deadline) >= 0;
}

void catUpdate(CatState& c, bool sleepHour, uint32_t nowMs) {
  switch (c.mood) {
    case CatMood::PETTED:
      if ((nowMs - c.since) >= PET_MS) {
        if (sleepHour) {
          // wake window runs from pet start
          uint32_t start = c.since;
          setMood(c, CatMood::WOKEN, nowMs);
          c.wakeUntil = start + NIGHT_WAKE_MS;
        } else {
          setMood(c, CatMood::IDLE, nowMs);
        }
      }
      break;
    case CatMood::WOKEN:
      if (reached(nowMs, c.wakeUntil))
        setMood(c, sleepHour ? CatMood::SLEEPING : CatMood::IDLE, nowMs);
      break;
    case CatMood::IDLE:
      if (sleepHour) setMood(c, CatMood::SLEEPING, nowMs);
      break;
    case CatMood::SLEEPING:
      if (!sleepHour) setMood(c, CatMood::IDLE, nowMs);
      break;
  }
}

void catOnWakeTap(CatState& c, uint32_t nowMs) {
  if (c.mood != CatMood::SLEEPING) return;
  setMood(c, CatMood::WOKEN, nowMs);
  c.wakeUntil = nowMs + NIGHT_WAKE_MS;
}

void catOnPet(CatState& c, uint32_t nowMs) { setMood(c, CatMood::PETTED, nowMs); }

bool catIsAsleep(const CatState& c) { return c.mood == CatMood::SLEEPING; }

bool catBlinking(const CatState& c, uint32_t nowMs) {
  if (c.mood != CatMood::IDLE && c.mood != CatMood::WOKEN) return false;
  uint32_t t = nowMs - c.since;
  return t >= BLINK_EVERY_MS && (t % BLINK_EVERY_MS) < BLINK_LEN_MS;
}
