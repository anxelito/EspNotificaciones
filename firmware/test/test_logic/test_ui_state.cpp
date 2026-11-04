#include <unity.h>
#include "../../src/logic/ui_state.h"

void test_ui_cycle_with_music() {
    UiState s;
    TEST_ASSERT_EQUAL(TapEffect::CHANGED_SCREEN, uiOnTap(s, true, false, 0));
    TEST_ASSERT_EQUAL(Screen::TASKS, s.screen);
    uiOnTap(s, true, false, 10);
    TEST_ASSERT_EQUAL(Screen::MUSIC, s.screen);
    uiOnTap(s, true, false, 20);
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_cycle_without_music() {
    UiState s;
    uiOnTap(s, false, false, 0);
    TEST_ASSERT_EQUAL(Screen::TASKS, s.screen);
    uiOnTap(s, false, false, 10);
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_wake_only_on_room() {
    UiState s;
    TEST_ASSERT_EQUAL(TapEffect::WOKE_CAT, uiOnTap(s, false, true, 0));
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
    s.screen = Screen::TASKS;
    TEST_ASSERT_EQUAL(TapEffect::CHANGED_SCREEN, uiOnTap(s, false, true, 10));
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_timeout_returns_room() {
    UiState s;
    uiOnTap(s, false, false, 1000);
    TEST_ASSERT_EQUAL(Screen::TASKS, s.screen);
    TEST_ASSERT_FALSE(uiTick(s, false, 29999 + 1000));
    TEST_ASSERT_TRUE(uiTick(s, false, 31000));
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_timeout_millis_wrap() {
    UiState s;
    uint32_t t = 0xFFFFF000u;
    uiOnTap(s, false, false, t);
    TEST_ASSERT_EQUAL(Screen::TASKS, s.screen);
    TEST_ASSERT_FALSE(uiTick(s, false, t + 29999u));
    TEST_ASSERT_TRUE(uiTick(s, false, t + 30000u));
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_music_ends_leaves_music() {
    UiState s;
    s.screen = Screen::MUSIC;
    s.lastTouchMs = 500;
    TEST_ASSERT_TRUE(uiTick(s, false, 510));
    TEST_ASSERT_EQUAL(Screen::ROOM, s.screen);
}
void test_ui_screen_count() {
    TEST_ASSERT_EQUAL(3, uiScreenCount(true));
    TEST_ASSERT_EQUAL(2, uiScreenCount(false));
}
