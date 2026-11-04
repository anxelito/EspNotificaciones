#include <unity.h>
#include "../../src/logic/room.h"

void test_classify_temp() {
    TEST_ASSERT_TRUE(classifyTemp(17.9f) == Comfort::COLD);
    TEST_ASSERT_TRUE(classifyTemp(18.0f) == Comfort::OK);
    TEST_ASSERT_TRUE(classifyTemp(25.0f) == Comfort::OK);
    TEST_ASSERT_TRUE(classifyTemp(25.1f) == Comfort::HOT);
    TEST_ASSERT_TRUE(classifyTemp(NAN) == Comfort::UNKNOWN);
}

void test_classify_humidity() {
    TEST_ASSERT_TRUE(classifyHumidity(29.9f) == HumidMood::DRY);
    TEST_ASSERT_TRUE(classifyHumidity(30.0f) == HumidMood::NORMAL);
    TEST_ASSERT_TRUE(classifyHumidity(65.0f) == HumidMood::NORMAL);
    TEST_ASSERT_TRUE(classifyHumidity(65.1f) == HumidMood::HUMID);
    TEST_ASSERT_TRUE(classifyHumidity(NAN) == HumidMood::UNKNOWN);
}

void test_sleep_window_wrap() {
    TEST_ASSERT_FALSE(isSleepHour(22));
    TEST_ASSERT_TRUE(isSleepHour(23));
    TEST_ASSERT_TRUE(isSleepHour(0));
    TEST_ASSERT_TRUE(isSleepHour(7));
    TEST_ASSERT_FALSE(isSleepHour(8));
    TEST_ASSERT_FALSE(isSleepHour(12));
}

void test_sleep_unknown_time() {
    TEST_ASSERT_FALSE(isSleepHour(-1));
}

void test_room_nan_keeps_last() {
    RoomReading r;
    roomAccept(r, 21.0f, 50.0f, 1000);
    roomAccept(r, NAN, NAN, 3000);
    TEST_ASSERT_EQUAL_FLOAT(21.0f, r.tempC);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, r.humidity);
    TEST_ASSERT_EQUAL_UINT32(1000, r.lastOkMs);
    TEST_ASSERT_TRUE(roomIsFresh(r, 20000));
}

void test_room_goes_stale() {
    RoomReading r;
    roomAccept(r, 21.0f, 50.0f, 1000);
    TEST_ASSERT_FALSE(roomIsFresh(r, 1000 + 30000));
}

void test_room_never_ok() {
    RoomReading r;
    TEST_ASSERT_FALSE(roomIsFresh(r, 0));
}
