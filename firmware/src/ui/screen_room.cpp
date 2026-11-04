// firmware/src/ui/screen_room.cpp
#include "screen_room.h"
#include <math.h>
#include "theme.h"
#include "../display.h"

static const int CARD_Y  = ROOM_STRIP_Y + 2;                 // 186
static const int CARD_H  = ROOM_STRIP_END - 2 - CARD_Y;      // 38 -> 186..224
static const int CARD_X  = MARGIN;
static const int CARD_W  = SCREEN_W - 2 * MARGIN;
static const int COL_W   = CARD_W / 3;
static const int VAL_Y   = CARD_Y + 2;                       // font 4, 26 px cell
static const int VAL_H   = 26;
static const int LBL_Y   = CARD_Y + CARD_H - 10;             // font 1, 8 px
static const int VAL_FONT = 4;

struct Shown {
    char     text[12];
    uint16_t col;
    bool     degree;     // draw a degree ring after the text
};
static Shown shown[3];
static bool  stripValid = false;

// Degree ring sized for font 4; returns advance in px.
static int drawDegree4(int x, int y, uint16_t col) {
    tft.drawCircle(x + 4, y + 6, 3, col);
    tft.drawCircle(x + 4, y + 6, 2, col);
    return 9;
}

static void drawValue(int col, const Shown& s) {
    const int x0 = CARD_X + col * COL_W;
    tft.fillRect(x0 + 2, VAL_Y, COL_W - 4, VAL_H, C_CARD);
    int w = tft.textWidth(s.text, VAL_FONT) + (s.degree ? 9 : 0);
    int x = x0 + COL_W / 2 - w / 2;
    tft.setTextColor(s.col, C_CARD);
    tft.drawString(s.text, x, VAL_Y, VAL_FONT);
    if (s.degree) drawDegree4(x + tft.textWidth(s.text, VAL_FONT), VAL_Y, s.col);
}

static void drawLabel(int col, const char* label) {
    const int x0 = CARD_X + col * COL_W;
    tft.setTextColor(C_MUTED, C_CARD);
    tft.drawString(label, x0 + COL_W / 2 - tft.textWidth(label, 1) / 2, LBL_Y, 1);
}

void drawRoomStrip(const RoomReading& r, bool fresh) {
    const bool haveT = fresh && r.everOk && !isnan(r.tempC);
    const bool haveH = fresh && r.everOk && !isnan(r.humidity);
    const Comfort comfort = haveT ? classifyTemp((float)lroundf(r.tempC)) : Comfort::UNKNOWN;

    Shown want[3];
    if (haveT) snprintf(want[0].text, sizeof(want[0].text), "%d", (int)lroundf(r.tempC));
    else       strlcpy(want[0].text, "--", sizeof(want[0].text));
    want[0].col = comfortColor(comfort);          // unknown -> green, "--°"
    want[0].degree = true;

    if (haveH) snprintf(want[1].text, sizeof(want[1].text), "%d%%", (int)lroundf(r.humidity));
    else       strlcpy(want[1].text, "--%", sizeof(want[1].text));
    want[1].col = haveH ? C_COLD : C_MUTED;
    want[1].degree = false;

    strlcpy(want[2].text, comfortWord(comfort), sizeof(want[2].text));
    want[2].col = comfort == Comfort::UNKNOWN ? C_MUTED : comfortColor(comfort);
    want[2].degree = false;

    if (!stripValid) {
        tft.fillRect(0, ROOM_STRIP_Y, SCREEN_W, ROOM_STRIP_END - ROOM_STRIP_Y, C_BG);
        tft.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, CARD_R, C_CARD);
        drawLabel(0, "TEMP");
        drawLabel(1, "HUMEDAD");
        drawLabel(2, "CONFORT");
    }
    for (int i = 0; i < 3; i++) {
        if (stripValid && shown[i].col == want[i].col && shown[i].degree == want[i].degree &&
            strcmp(shown[i].text, want[i].text) == 0) continue;
        drawValue(i, want[i]);
        shown[i] = want[i];
    }
    stripValid = true;
}

void drawRoomScreen() {
    tft.fillRect(0, HEADER_H, SCREEN_W, ROOM_STRIP_END - HEADER_H, C_BG);
    stripValid = false;
}
