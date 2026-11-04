// firmware/src/logic/cat_behavior.h -- host-compilable, no Arduino includes
//
// What the awake cat is doing right now, chosen at random between rests:
// walk across the room, look around, groom, yawn, stretch or hop. Only runs
// while the cat is awake and idle (canAct); otherwise it just sits where it is.
#pragma once
#include <cstdint>

enum class CatAct : uint8_t { SIT, WALK, LOOK, GROOM, YAWN, STRETCH, HOP };

struct CatBehavior {
  CatAct   act    = CatAct::SIT;
  uint32_t since  = 0;   // when act began
  uint32_t dur    = 0;   // act length in ms (WALK: derived from distance)
  int      x      = 0;   // current left x of the cat sprite
  int      fromX  = 0;   // WALK start
  int      toX    = 0;   // WALK target
  int8_t   facing = 1;   // -1 left, +1 right
  int8_t   look   = 0;   // LOOK direction: -1 / +1
  uint32_t rng    = 1;   // xorshift32 state, never 0
};

void   behaviorInit(CatBehavior& b, int x, uint32_t seed, uint32_t nowMs);
// Advances the behaviour; x stays within [minX, maxX].
void   behaviorTick(CatBehavior& b, uint32_t nowMs, bool canAct, int minX, int maxX);
// Progress of the current act, 0..1.
float  behaviorPhase(const CatBehavior& b, uint32_t nowMs);
