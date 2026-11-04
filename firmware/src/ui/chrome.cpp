// firmware/src/ui/chrome.cpp
#include "chrome.h"
#include "theme.h"
#include "../display.h"

// Text runs use font 2 (ASCII only), so degree ring / middle dot / note are
// drawn with primitives between runs.
static const int FONT = 2;

// Draws a degree ring at x (left edge), returns advance in px.
static int drawDegree(int x, int y, uint16_t col) {
    tft.drawCircle(x + 2, y + 4, 2, col);
    return 6;
}

static int drawMidDot(int x, int y, uint16_t col) {
    tft.fillCircle(x + 3, y + 8, 1, col);
    return 8;
}

// Small eighth-note: head + stem + flag, ~8x11 px, top-left at (x,y).
static void drawNote(int x, int y, uint16_t col) {
    tft.fillCircle(x + 2, y + 8, 2, col);
    tft.drawFastVLine(x + 4, y, 9, col);
    tft.drawLine(x + 4, y, x + 7, y + 3, col);
    tft.drawLine(x + 4, y + 1, x + 7, y + 4, col);
}

// Draws title, turning TITLE_DOT bytes into a primitive middle dot.
static void drawTitle(const char* title, int x, int y, uint16_t col) {
    char run[32];
    size_t n = 0;
    tft.setTextColor(col, C_BG);
    for (const char* p = title;; p++) {
        if (*p == '\0' || *p == '\x01' || n >= sizeof(run) - 1) {
            run[n] = '\0';
            if (n) { tft.drawString(run, x, y, FONT); x += tft.textWidth(run, FONT); }
            n = 0;
            if (*p == '\0') return;
            if (*p == '\x01') x += drawMidDot(x, y, col);
            continue;
        }
        run[n++] = *p;
    }
}

void drawHeader(const HeaderInfo& h) {
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_BG);
    const int ty = (HEADER_H - 16) / 2;   // font 2 is ~16 px tall

    // Title (left)
    drawTitle(h.title ? h.title : "", MARGIN + 2, ty, C_MUTED);

    // Centre: time or OFFLINE, note after it while playing.
    // Time is drawn as "HH", ":", "MM" with COLON_PAD px either side of the
    // colon, because font 2's colon glyph hugs the digits.
    static const int COLON_PAD = 3;
    int cw, cx;
    if (!h.online) {
        cw = tft.textWidth("OFFLINE", FONT);
        cx = SCREEN_W / 2 - cw / 2 - (h.playing ? 6 : 0);
        tft.setTextColor(C_URGENT, C_BG);
        tft.drawString("OFFLINE", cx, ty, FONT);
    } else {
        char hh[4], mm[4];
        if (h.timeKnown) {
            snprintf(hh, sizeof(hh), "%02d", h.hour);
            snprintf(mm, sizeof(mm), "%02d", h.minute);
        } else {
            strlcpy(hh, "--", sizeof(hh));
            strlcpy(mm, "--", sizeof(mm));
        }
        int hw = tft.textWidth(hh, FONT);
        int colw = tft.textWidth(":", FONT);
        int mw = tft.textWidth(mm, FONT);
        cw = hw + COLON_PAD + colw + COLON_PAD + mw;
        cx = SCREEN_W / 2 - cw / 2 - (h.playing ? 6 : 0);
        tft.setTextColor(C_TIME, C_BG);
        int x = cx;
        tft.drawString(hh, x, ty, FONT);  x += hw + COLON_PAD;
        tft.drawString(":", x, ty, FONT); x += colw + COLON_PAD;
        tft.drawString(mm, x, ty, FONT);
    }
    if (h.playing) drawNote(cx + cw + 6, ty + 2, C_OK);

    // Right: "22° · 48%" or "--° · --%"
    bool have = h.room && h.room->everOk && h.roomFresh;
    char tb[8], hb[8];
    uint16_t tcol = C_MUTED;
    if (have) {
        snprintf(tb, sizeof(tb), "%d", (int)lroundf(h.room->tempC));
        snprintf(hb, sizeof(hb), "%d%%", (int)lroundf(h.room->humidity));
        tcol = comfortColor(classifyTemp((float)lroundf(h.room->tempC)));
    } else {
        strlcpy(tb, "--", sizeof(tb));
        strlcpy(hb, "--%", sizeof(hb));
    }
    int total = tft.textWidth(tb, FONT) + 6 + 8 + tft.textWidth(hb, FONT);
    int x = SCREEN_W - MARGIN - 2 - total;
    tft.setTextColor(tcol, C_BG);
    tft.drawString(tb, x, ty, FONT);
    x += tft.textWidth(tb, FONT);
    x += drawDegree(x, ty, tcol);
    x += drawMidDot(x, ty, C_MUTED);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString(hb, x, ty, FONT);
}

void drawPageDots(int index, int count) {
    const int r = 3, gap = 8;
    const int stripY = SCREEN_H - 14;
    tft.fillRect(0, stripY, SCREEN_W, 14, C_BG);
    int total = count * 2 * r + (count - 1) * gap;
    int x = SCREEN_W / 2 - total / 2 + r;
    int y = SCREEN_H - 8;
    for (int i = 0; i < count; i++) {
        tft.fillCircle(x, y, r, i == index ? C_TIME : C_DOT_OFF);
        x += 2 * r + gap;
    }
}
