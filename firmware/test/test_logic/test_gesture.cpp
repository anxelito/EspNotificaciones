#include <unity.h>
#include "../../src/logic/gesture.h"

void test_tap() {
    GestureDetector g;
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(true, 0));
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(true, 100));
    TEST_ASSERT_EQUAL(Gesture::TAP, g.update(false, 150));
}
void test_bounce_ignored() {
    GestureDetector g;
    g.update(true, 0);
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(false, 10));
}
void test_long_press_fires_while_held() {
    GestureDetector g;
    g.update(true, 0);
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(true, 999));
    TEST_ASSERT_EQUAL(Gesture::LONG_PRESS, g.update(true, 1000));
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(true, 1500));
}
void test_long_press_no_tap_on_release() {
    GestureDetector g;
    g.update(true, 0);
    g.update(true, 1000);
    g.update(true, 1500);
    TEST_ASSERT_EQUAL(Gesture::NONE, g.update(false, 1600));
}
void test_gesture_millis_wrap() {
    GestureDetector g;
    g.update(true, 0xFFFFFF00u);
    TEST_ASSERT_EQUAL(Gesture::TAP, g.update(false, 0x00000040u));
}
void test_gesture_millis_wrap_long() {
    GestureDetector g;
    g.update(true, 0xFFFFFF00u);
    TEST_ASSERT_EQUAL(Gesture::LONG_PRESS, g.update(true, 0x000002F0u));
}
