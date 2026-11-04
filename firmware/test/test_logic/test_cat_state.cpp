#include <unity.h>
#include "../../src/logic/cat_state.h"

void test_cat_falls_asleep() {
    CatState c;
    catUpdate(c, true, 0);
    TEST_ASSERT_EQUAL(CatMood::SLEEPING, c.mood);
    TEST_ASSERT_TRUE(catIsAsleep(c));
}
void test_cat_wakes_morning() {
    CatState c;
    c.mood = CatMood::SLEEPING;
    catUpdate(c, false, 100);
    TEST_ASSERT_EQUAL(CatMood::IDLE, c.mood);
}
void test_cat_night_tap_window() {
    CatState c;
    c.mood = CatMood::SLEEPING;
    catOnWakeTap(c, 0);
    TEST_ASSERT_EQUAL(CatMood::WOKEN, c.mood);
    catUpdate(c, true, 9999);
    TEST_ASSERT_EQUAL(CatMood::WOKEN, c.mood);
    catUpdate(c, true, 10000);
    TEST_ASSERT_EQUAL(CatMood::SLEEPING, c.mood);
}
void test_cat_pet_day() {
    CatState c;
    catOnPet(c, 0);
    catUpdate(c, false, 1999);
    TEST_ASSERT_EQUAL(CatMood::PETTED, c.mood);
    catUpdate(c, false, 2000);
    TEST_ASSERT_EQUAL(CatMood::IDLE, c.mood);
}
void test_cat_pet_night() {
    CatState c;
    c.mood = CatMood::SLEEPING;
    catOnPet(c, 0);
    TEST_ASSERT_EQUAL(CatMood::PETTED, c.mood);
    catUpdate(c, true, 2000);
    TEST_ASSERT_EQUAL(CatMood::WOKEN, c.mood);
    catUpdate(c, true, 10000);
    TEST_ASSERT_EQUAL(CatMood::SLEEPING, c.mood);
}
void test_cat_no_blink_when_sleeping() {
    CatState c;
    c.mood = CatMood::SLEEPING;
    TEST_ASSERT_FALSE(catBlinking(c, 0));
    TEST_ASSERT_FALSE(catBlinking(c, 4000));
    TEST_ASSERT_FALSE(catBlinking(c, 4100));
}
void test_cat_blink_period() {
    CatState c;
    TEST_ASSERT_FALSE(catBlinking(c, 0));
    TEST_ASSERT_TRUE(catBlinking(c, 4000));
    TEST_ASSERT_TRUE(catBlinking(c, 4100));
    TEST_ASSERT_FALSE(catBlinking(c, 4200));
}
