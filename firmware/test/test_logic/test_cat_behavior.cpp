#include <unity.h>
#include "../../src/logic/cat_behavior.h"
#include "../../src/config.h"

static const int MINX = 8, MAXX = 212;

// Run the behaviour in 80 ms frames for `ms`, checking x bounds every frame.
static void run(CatBehavior& b, uint32_t& now, uint32_t ms, bool canAct, bool* sawWalk = nullptr) {
    for (uint32_t t = 0; t < ms; t += 80) {
        now += 80;
        behaviorTick(b, now, canAct, MINX, MAXX);
        TEST_ASSERT_TRUE(b.x >= MINX && b.x <= MAXX);
        if (sawWalk && b.act == CatAct::WALK) *sawWalk = true;
    }
}

void test_behavior_starts_sitting() {
    CatBehavior b;
    behaviorInit(b, 110, 1234, 0);
    TEST_ASSERT_EQUAL(CatAct::SIT, b.act);
    TEST_ASSERT_EQUAL(110, b.x);
}

void test_behavior_acts_and_walks_in_bounds() {
    CatBehavior b;
    behaviorInit(b, 110, 42, 0);
    uint32_t now = 0;
    bool walked = false;
    run(b, now, 10UL * 60UL * 1000UL, true, &walked);   // 10 simulated minutes
    TEST_ASSERT_TRUE(walked);
}

void test_behavior_walk_reaches_target() {
    CatBehavior b;
    behaviorInit(b, 8, 7, 0);
    b.act = CatAct::WALK; b.since = 0; b.fromX = 8; b.toX = 100; b.facing = 1;
    behaviorTick(b, 1000, true, MINX, MAXX);
    TEST_ASSERT_EQUAL(8 + (int)CAT_WALK_PX_PER_S, b.x);
    behaviorTick(b, 10000, true, MINX, MAXX);
    TEST_ASSERT_EQUAL(100, b.x);
    TEST_ASSERT_EQUAL(CatAct::SIT, b.act);
}

void test_behavior_frozen_when_cannot_act() {
    CatBehavior b;
    behaviorInit(b, 50, 99, 0);
    b.act = CatAct::WALK; b.since = 0; b.fromX = 50; b.toX = 200; b.facing = 1;
    uint32_t now = 0;
    run(b, now, 60000, false);
    TEST_ASSERT_EQUAL(CatAct::SIT, b.act);
    TEST_ASSERT_EQUAL(50, b.x);
}

void test_behavior_phase_range() {
    CatBehavior b;
    behaviorInit(b, 50, 5, 0);
    b.act = CatAct::YAWN; b.since = 1000; b.dur = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, behaviorPhase(b, 1500));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, behaviorPhase(b, 5000));
}

void test_behavior_millis_wrap() {
    CatBehavior b;
    uint32_t now = 0xFFFFFFFFu - 5000u;
    behaviorInit(b, 110, 3, now);
    run(b, now, 60000, true);   // crosses the 32-bit wrap
}
