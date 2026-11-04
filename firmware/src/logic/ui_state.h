// firmware/src/logic/ui_state.h -- host-compilable, no Arduino includes
#pragma once
#include <cstdint>

enum class Screen { ROOM, TASKS, MUSIC };
enum class TapEffect { CHANGED_SCREEN, WOKE_CAT };

struct UiState {
  Screen   screen      = Screen::ROOM;
  uint32_t lastTouchMs = 0;
};

TapEffect uiOnTap(UiState& s, bool musicPlaying, bool catAsleep, uint32_t nowMs);
bool      uiTick(UiState& s, bool musicPlaying, uint32_t nowMs);  // true if screen changed
int       uiScreenCount(bool musicPlaying);                        // 3 or 2
int       uiScreenIndex(Screen s);                                 // ROOM 0, TASKS 1, MUSIC 2
