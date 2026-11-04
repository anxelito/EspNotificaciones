// firmware/src/logic/gesture.h -- host-compilable, no Arduino includes
#pragma once
#include <cstdint>

enum class Gesture { NONE, TAP, LONG_PRESS };

class GestureDetector {
 public:
  Gesture update(bool pressed, uint32_t nowMs);  // call every loop
 private:
  bool     down_      = false;
  bool     longFired_ = false;
  uint32_t startMs_   = 0;
};
