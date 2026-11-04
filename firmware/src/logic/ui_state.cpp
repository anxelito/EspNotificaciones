#include "ui_state.h"
#include "../config.h"

TapEffect uiOnTap(UiState& s, bool musicPlaying, bool catAsleep, uint32_t nowMs) {
  s.lastTouchMs = nowMs;
  if (s.screen == Screen::ROOM && catAsleep) return TapEffect::WOKE_CAT;
  switch (s.screen) {
    case Screen::ROOM:  s.screen = Screen::TASKS; break;
    case Screen::TASKS: s.screen = musicPlaying ? Screen::MUSIC : Screen::ROOM; break;
    case Screen::MUSIC: s.screen = Screen::ROOM; break;
  }
  return TapEffect::CHANGED_SCREEN;
}

bool uiTick(UiState& s, bool musicPlaying, uint32_t nowMs) {
  if (s.screen == Screen::ROOM) return false;
  // Music screen stays up while something plays; it leaves when music stops.
  bool musicUp = s.screen == Screen::MUSIC && musicPlaying;
  if ((!musicUp && (nowMs - s.lastTouchMs) >= IDLE_RETURN_MS) ||
      (s.screen == Screen::MUSIC && !musicPlaying)) {
    s.screen = Screen::ROOM;
    return true;
  }
  return false;
}

int uiScreenCount(bool musicPlaying) { return musicPlaying ? 3 : 2; }

int uiScreenIndex(Screen s) {
  switch (s) {
    case Screen::ROOM:  return 0;
    case Screen::TASKS: return 1;
    default:            return 2;
  }
}
