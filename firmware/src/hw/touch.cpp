#include "touch.h"
#include <Arduino.h>
#include "../config.h"

void touchInit() {
    pinMode(PIN_TOUCH, INPUT);
}

bool touchPressed() {
    return digitalRead(PIN_TOUCH) == HIGH;
}
