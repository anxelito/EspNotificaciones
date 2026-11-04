#include "room.h"
#include "../config.h"

Comfort classifyTemp(float c) {
    if (std::isnan(c)) return Comfort::UNKNOWN;
    if (c < TEMP_COLD_BELOW) return Comfort::COLD;
    if (c > TEMP_HOT_ABOVE) return Comfort::HOT;
    return Comfort::OK;
}

HumidMood classifyHumidity(float pct) {
    if (std::isnan(pct)) return HumidMood::UNKNOWN;
    if (pct < HUM_DRY_BELOW) return HumidMood::DRY;
    if (pct > HUM_HUMID_ABOVE) return HumidMood::HUMID;
    return HumidMood::NORMAL;
}

bool isSleepHour(int hour) {
    if (hour < 0) return false;
    return hour >= SLEEP_START_H || hour < SLEEP_END_H;
}

void roomAccept(RoomReading& r, float t, float h, uint32_t nowMs) {
    if (std::isnan(t) || std::isnan(h)) return;
    r.tempC = t;
    r.humidity = h;
    r.lastOkMs = nowMs;
    r.everOk = true;
}

bool roomIsFresh(const RoomReading& r, uint32_t nowMs) {
    return r.everOk && (uint32_t)(nowMs - r.lastOkMs) < SENSOR_STALE_MS;
}
