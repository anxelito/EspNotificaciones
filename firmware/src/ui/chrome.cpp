// firmware/src/ui/chrome.cpp
#include "chrome.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "../display.h"

// Small eighth-note: head + stem + flag, ~8x11 px, top-left at (x,y).
static void drawNote(int x, int y, uint16_t col) {
    tft.fillCircle(x + 2, y + 8, 2, col);
    tft.drawFastVLine(x + 4, y, 9, col);
    tft.drawLine(x + 4, y, x + 7, y + 3, col);
    tft.drawLine(x + 4, y + 1, x + 7, y + 4, col);
}

// Envelope icon ~12x9, top-left at (x,y).
static void drawEnvelope(int x, int y, uint16_t col, uint16_t bg) {
    tft.fillRoundRect(x, y, 13, 9, 1, col);
    tft.drawLine(x + 1, y + 1, x + 6, y + 5, bg);
    tft.drawLine(x + 11, y + 1, x + 6, y + 5, bg);
}

void drawHeader(const HeaderInfo& h) {
    const uint16_t bg = h.bg;
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, bg);
    const int ty = (HEADER_H - fontLineH(UiFont::SMALL)) / 2 + 1;

    // Title (left)
    drawText(h.title ? h.title : "", MARGIN + 2, ty, UiFont::SMALL, C_MUTED, bg);

    // Centre: time or OFFLINE, note after it while playing.
    char buf[8];
    uint16_t col = C_TIME;
    if (!h.online) {
        strlcpy(buf, "OFFLINE", sizeof(buf));
        col = C_URGENT;
    } else if (h.timeKnown) {
        snprintf(buf, sizeof(buf), "%02d:%02d", h.hour, h.minute);
    } else {
        strlcpy(buf, "--:--", sizeof(buf));
    }
    int cw = textWidthIn(buf, UiFont::SMALL);
    int cx = SCREEN_W / 2 - cw / 2 - (h.playing ? 6 : 0);
    drawText(buf, cx, ty, UiFont::SMALL, col, bg);
    if (h.playing) drawNote(cx + cw + 6, ty + 3, C_OK);

    // Right: "22° · 48%" or "--° · --%"
    bool have = h.room && h.room->everOk && h.roomFresh;
    char tb[12], hb[12];
    uint16_t tcol = C_MUTED;
    if (have) {
        snprintf(tb, sizeof(tb), "%d°", (int)lroundf(h.room->tempC));
        snprintf(hb, sizeof(hb), " · %d%%", (int)lroundf(h.room->humidity));
        tcol = comfortColor(classifyTemp((float)lroundf(h.room->tempC)));
    } else {
        strlcpy(tb, "--°", sizeof(tb));
        strlcpy(hb, " · --%", sizeof(hb));
    }
    int rx = SCREEN_W - MARGIN - 2;
    int hw = textWidthIn(hb, UiFont::SMALL);
    drawText(hb, rx - hw, ty, UiFont::SMALL, C_MUTED, bg);
    drawTextRight(tb, rx - hw, ty, UiFont::SMALL, tcol, bg);
}

void drawToast(const char* label, const char* text, uint16_t accent) {
    const uint16_t bg = mix565(accent, C_BG, 70);
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_BG);
    tft.fillRoundRect(4, 1, SCREEN_W - 8, HEADER_H - 2, 8, bg);
    const int ty = (HEADER_H - fontLineH(UiFont::SMALL)) / 2 + 1;
    drawEnvelope(12, 9, accent, bg);
    int x = 32;
    drawText(label, x, ty, UiFont::SMALL, accent, bg);
    x += textWidthIn(label, UiFont::SMALL) + 8;
    char buf[96];
    fitText(text, buf, sizeof(buf), SCREEN_W - 12 - x, UiFont::SMALL);
    drawText(buf, x, ty, UiFont::SMALL, C_TEXT, bg);
}

void drawPageDots(int index, int count, uint16_t bg) {
    const int r = 3, gap = 8;
    const int stripY = SCREEN_H - 14;
    tft.fillRect(0, stripY, SCREEN_W, 14, bg);
    int total = count * 2 * r + (count - 1) * gap;
    int x = SCREEN_W / 2 - total / 2 + r;
    int y = SCREEN_H - 8;
    for (int i = 0; i < count; i++) {
        tft.fillCircle(x, y, r, i == index ? C_TIME : mix565(C_DOT_OFF, bg, 200));
        x += 2 * r + gap;
    }
}
