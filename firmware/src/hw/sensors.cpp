#include "sensors.h"
#include <Arduino.h>
#include <DHT.h>
#include "../config.h"

static DHT dht(PIN_DHT, DHT11);
static uint32_t lastReadMs = 0;
static bool     readOnce   = false;

void sensorsInit() {
    dht.begin();
}

void sensorsPoll(RoomReading& r, uint32_t nowMs) {
    if (readOnce && (nowMs - lastReadMs) < DHT_READ_MS) return;
    readOnce = true;
    lastReadMs = nowMs;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    roomAccept(r, t, h, nowMs);
}
