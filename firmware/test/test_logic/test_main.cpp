#include <unity.h>

// Add new test files by declaring their tests here and registering in main().
void test_classify_temp();
void test_classify_humidity();
void test_sleep_window_wrap();
void test_sleep_unknown_time();
void test_room_nan_keeps_last();
void test_room_goes_stale();
void test_room_never_ok();

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_classify_temp);
    RUN_TEST(test_classify_humidity);
    RUN_TEST(test_sleep_window_wrap);
    RUN_TEST(test_sleep_unknown_time);
    RUN_TEST(test_room_nan_keeps_last);
    RUN_TEST(test_room_goes_stale);
    RUN_TEST(test_room_never_ok);
    return UNITY_END();
}
