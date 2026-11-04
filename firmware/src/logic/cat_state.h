// firmware/src/logic/cat_state.h -- host-compilable, no Arduino includes
#pragma once
#include <cstdint>

enum class CatMood { IDLE, SLEEPING, WOKEN, PETTED };

struct CatState {
  CatMood  mood      = CatMood::IDLE;
  uint32_t since     = 0;  // when the current mood began
  uint32_t wakeUntil = 0;  // end of WOKEN window (valid while WOKEN/PETTED at night)
};

void catUpdate(CatState& c, bool sleepHour, uint32_t nowMs);
void catOnWakeTap(CatState& c, uint32_t nowMs);      // SLEEPING -> WOKEN for NIGHT_WAKE_MS
void catOnPet(CatState& c, uint32_t nowMs);          // any -> PETTED for PET_MS
bool catIsAsleep(const CatState& c);
bool catBlinking(const CatState& c, uint32_t nowMs); // awake eyes-open moods only
