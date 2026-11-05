#include "touch.h"
#include <Arduino.h>
#include "../config.h"

void touchInit() {
    // Pull-down so an unconnected TTP223 input reads "not touched" instead of
    // floating into phantom taps (TTP223 output is push-pull, so harmless).
    pinMode(PIN_TOUCH, INPUT_PULLDOWN);
#if PIN_BOOT_BTN >= 0
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
#endif
}

bool touchPressed() {
    bool pressed = digitalRead(PIN_TOUCH) == HIGH;
#if SENSOR_DEBUG
    static int last = -1;
    if ((int)pressed != last) {
        if (last >= 0 || pressed)
            Serial.printf("[touch] GPIO%d %s\n", PIN_TOUCH, pressed ? "ALTO (tocando)" : "BAJO (suelto)");
        else
            Serial.printf("[touch] GPIO%d BAJO al arrancar (ok si no lo tocas)\n", PIN_TOUCH);
        last = pressed;
    }
#endif
#if PIN_BOOT_BTN >= 0
    pressed = pressed || digitalRead(PIN_BOOT_BTN) == LOW;  // BOOT is active-low
#endif
    return pressed;
}
