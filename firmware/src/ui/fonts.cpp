// firmware/src/ui/fonts.cpp
#include "fonts.h"
#include "font_data.h"
#include "../display.h"

static const uint8_t* const FONT_ARRAYS[] = { FONT_SMALL, FONT_BODY, FONT_TITLE, FONT_CLOCK };
// maxAscent + maxDescent of each generated font (see tools/make_fonts.py).
static const int LINE_H[] = { 19, 23, 32, 34 };

static int current = -1;

int useFont(UiFont f) {
    int i = (int)f;
    if (i != current) {
        tft.loadFont(FONT_ARRAYS[i]);   // unloads the previous one first
        current = i;
    }
    return tft.fontHeight();
}

void useBuiltinFont() {
    if (current >= 0) tft.unloadFont();
    current = -1;
}

int fontLineH(UiFont f) { return LINE_H[(int)f]; }
