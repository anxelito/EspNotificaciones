#include "backlight.h"
#include <Arduino.h>
#include "../config.h"

#define BL_CHANNEL 4
#define BL_FREQ    5000
#define BL_BITS    8

void backlightInit() {
#if BACKLIGHT_PIN >= 0
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_BITS);
    ledcAttachPin(BACKLIGHT_PIN, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 255);
#endif
}

void backlightSet(uint8_t level) {
#if BACKLIGHT_PIN >= 0
    ledcWrite(BL_CHANNEL, level);
#else
    (void)level;
#endif
}
