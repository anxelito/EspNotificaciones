#include "sensors.h"
#include <Arduino.h>
#include <DHT.h>
#include "../config.h"

static DHT dht(PIN_DHT, DHT11);
static uint32_t lastReadMs = 0;
static bool     readOnce   = false;

void sensorsInit() {
#if SENSOR_DEBUG
    // Line check before the library takes the pin: with the internal
    // pull-down on, only a powered module (it has its own pull-up) pulls
    // DATA high.
    pinMode(PIN_DHT, INPUT_PULLDOWN);
    delay(5);
    bool high = digitalRead(PIN_DHT) == HIGH;
    Serial.printf("[dht] GPIO%d en reposo: %s\n", PIN_DHT,
                  high ? "ALTO (ok: el modulo esta alimentado y conectado)"
                       : "BAJO (no llega el modulo: revisa DATA->GPIO27, VCC->3V3 y GND)");
#endif
    dht.begin();
}

void sensorsPoll(RoomReading& r, uint32_t nowMs) {
    if (readOnce && (nowMs - lastReadMs) < DHT_READ_MS) return;
    readOnce = true;
    lastReadMs = nowMs;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
#if SENSOR_DEBUG
    if (isnan(t) || isnan(h))
        Serial.println("[dht] sin respuesta del sensor (revisa cables; DATA en GPIO27)");
    else
        Serial.printf("[dht] %.1f C  %.0f %%\n", t, h);
#endif
    roomAccept(r, t, h, nowMs);
}
