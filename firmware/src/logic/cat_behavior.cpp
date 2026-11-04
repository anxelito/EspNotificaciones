#include "cat_behavior.h"
#include "../config.h"

static uint32_t nextRand(CatBehavior& b) {
  uint32_t x = b.rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  b.rng = x ? x : 1;
  return b.rng;
}

static uint32_t randRange(CatBehavior& b, uint32_t lo, uint32_t hi) {  // [lo, hi]
  return lo + nextRand(b) % (hi - lo + 1);
}

static void start(CatBehavior& b, CatAct a, uint32_t dur, uint32_t nowMs) {
  b.act = a;
  b.since = nowMs;
  b.dur = dur;
}

static void rest(CatBehavior& b, uint32_t nowMs) {
  start(b, CatAct::SIT, randRange(b, CAT_REST_MIN_MS, CAT_REST_MAX_MS), nowMs);
}

static int clampX(int x, int minX, int maxX) {
  return x < minX ? minX : (x > maxX ? maxX : x);
}

static void pickNext(CatBehavior& b, uint32_t nowMs, int minX, int maxX) {
  uint32_t r = nextRand(b) % 100;
  if (r < 30 && maxX - minX >= CAT_WALK_MIN_PX) {
    int to = (int)randRange(b, (uint32_t)minX, (uint32_t)maxX);
    if ((to > b.x ? to - b.x : b.x - to) < CAT_WALK_MIN_PX)   // too short: go to the far side
      to = (b.x - minX > maxX - b.x) ? minX : maxX;
    b.fromX = b.x;
    b.toX = to;
    b.facing = to > b.x ? 1 : -1;
    int dist = to > b.x ? to - b.x : b.x - to;
    start(b, CatAct::WALK, (uint32_t)dist * 1000u / CAT_WALK_PX_PER_S, nowMs);
  } else if (r < 52) {
    b.look = (nextRand(b) & 1) ? 1 : -1;
    start(b, CatAct::LOOK, CAT_LOOK_MS, nowMs);
  } else if (r < 68) {
    start(b, CatAct::GROOM, CAT_GROOM_MS, nowMs);
  } else if (r < 80) {
    start(b, CatAct::YAWN, CAT_YAWN_MS, nowMs);
  } else if (r < 90) {
    start(b, CatAct::STRETCH, CAT_STRETCH_MS, nowMs);
  } else {
    start(b, CatAct::HOP, CAT_HOP_MS, nowMs);
  }
}

void behaviorInit(CatBehavior& b, int x, uint32_t seed, uint32_t nowMs) {
  b = CatBehavior();
  b.x = x;
  b.rng = seed ? seed : 0x9E3779B9u;
  rest(b, nowMs);
}

void behaviorTick(CatBehavior& b, uint32_t nowMs, bool canAct, int minX, int maxX) {
  b.x = clampX(b.x, minX, maxX);
  if (!canAct) {
    if (b.act != CatAct::SIT) rest(b, nowMs);
    else b.since = nowMs;             // rest restarts once the cat may act again
    return;
  }

  uint32_t el = nowMs - b.since;
  if (b.act == CatAct::WALK) {
    int dist = b.toX > b.fromX ? b.toX - b.fromX : b.fromX - b.toX;
    uint32_t moved = el * CAT_WALK_PX_PER_S / 1000u;
    if (moved >= (uint32_t)dist) {
      b.x = clampX(b.toX, minX, maxX);
      rest(b, nowMs);
    } else {
      b.x = clampX(b.fromX + b.facing * (int)moved, minX, maxX);
    }
    return;
  }

  if (el >= b.dur) {
    if (b.act == CatAct::SIT) pickNext(b, nowMs, minX, maxX);
    else rest(b, nowMs);
  }
}

float behaviorPhase(const CatBehavior& b, uint32_t nowMs) {
  if (b.dur == 0) return 1.0f;
  uint32_t el = nowMs - b.since;
  return el >= b.dur ? 1.0f : (float)el / (float)b.dur;
}
