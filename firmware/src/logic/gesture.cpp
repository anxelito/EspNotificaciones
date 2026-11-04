#include "gesture.h"
#include "../config.h"

Gesture GestureDetector::update(bool pressed, uint32_t nowMs) {
  if (pressed) {
    if (!down_) {
      down_ = true;
      longFired_ = false;
      startMs_ = nowMs;
      return Gesture::NONE;
    }
    if (!longFired_ && (nowMs - startMs_) >= LONG_PRESS_MS) {
      longFired_ = true;
      return Gesture::LONG_PRESS;
    }
    return Gesture::NONE;
  }
  if (!down_) return Gesture::NONE;
  down_ = false;
  if (longFired_) return Gesture::NONE;
  return (nowMs - startMs_) >= TOUCH_DEBOUNCE_MS ? Gesture::TAP : Gesture::NONE;
}
