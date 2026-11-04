// firmware/src/logic/room.h -- host-compilable, no Arduino includes
#pragma once
#include <cmath>
#include <cstdint>

enum class Comfort   { UNKNOWN, COLD, OK, HOT };
enum class HumidMood { UNKNOWN, NORMAL, DRY, HUMID };

Comfort   classifyTemp(float c);
HumidMood classifyHumidity(float pct);
bool      isSleepHour(int hour);

struct RoomReading {
    float    tempC     = NAN;
    float    humidity  = NAN;
    uint32_t lastOkMs  = 0;
    bool     everOk    = false;
};

void roomAccept(RoomReading& r, float t, float h, uint32_t nowMs);
bool roomIsFresh(const RoomReading& r, uint32_t nowMs);
