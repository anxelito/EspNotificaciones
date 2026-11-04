// firmware/src/ui/text_fit.cpp
#include "text_fit.h"
#include <Arduino.h>
#include <string.h>
#include "../display.h"

void fitText(const char* src, char* out, size_t outLen, int maxPx, int font) {
    strlcpy(out, src, outLen);
    if (tft.textWidth(out, font) <= maxPx) return;
    size_t n = strlen(out);
    while (n > 1) {
        n--;
        while (n > 1 && (((uint8_t)out[n]) & 0xC0) == 0x80) n--;
        out[n] = '\0';
        char tmp[72];
        snprintf(tmp, sizeof(tmp), "%s~", out);
        if (tft.textWidth(tmp, font) <= maxPx) { strlcpy(out, tmp, outLen); return; }
    }
}
