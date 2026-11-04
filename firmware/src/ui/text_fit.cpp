// firmware/src/ui/text_fit.cpp
#include "text_fit.h"
#include <Arduino.h>
#include <string.h>
#include "../display.h"

static const char ELLIPSIS[] = "...";
static const size_t TEXT_BUF_MAX = 128;

static bool isCont(char c) { return (((uint8_t)c) & 0xC0) == 0x80; }

// Width of the first n bytes of s in the current font.
static int prefixWidth(const char* s, size_t n) {
    char tmp[TEXT_BUF_MAX];
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    return tft.textWidth(tmp);
}

int textWidthIn(const char* s, UiFont f) {
    useFont(f);
    return tft.textWidth(s);
}

void drawText(const char* s, int x, int y, UiFont f, uint16_t fg, uint16_t bg) {
    useFont(f);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(s, x, y);
}

void drawTextCentre(const char* s, int cx, int y, UiFont f, uint16_t fg, uint16_t bg) {
    drawText(s, cx - textWidthIn(s, f) / 2, y, f, fg, bg);
}

void drawTextRight(const char* s, int rx, int y, UiFont f, uint16_t fg, uint16_t bg) {
    drawText(s, rx - textWidthIn(s, f), y, f, fg, bg);
}

void fitText(const char* src, char* out, size_t outLen, int maxPx, UiFont f) {
    useFont(f);
    strlcpy(out, src, outLen);
    if (tft.textWidth(out) <= maxPx) return;
    size_t n = strlen(out);
    while (n > 1) {
        n--;
        while (n > 1 && isCont(out[n])) n--;
        out[n] = '\0';
        char tmp[TEXT_BUF_MAX];
        snprintf(tmp, sizeof(tmp), "%s%s", out, ELLIPSIS);
        if (tft.textWidth(tmp) <= maxPx) { strlcpy(out, tmp, outLen); return; }
    }
}

// Bytes of s (no leading spaces) that go on one line: the longest prefix
// that fits, cut back to the last space when a word would be split.
static size_t lineTake(const char* s, int maxPx) {
    size_t len = strlen(s), best = 0, lastSpace = 0;
    for (size_t i = 1; i <= len && i < TEXT_BUF_MAX; i++) {
        if (i < len && isCont(s[i])) continue;      // keep UTF-8 sequences whole
        if (prefixWidth(s, i) > maxPx) break;
        best = i;
        if (i < len && s[i] == ' ') lastSpace = i;
    }
    if (best == 0) {                                // a single glyph wider than the line
        best = 1;
        while (best < len && isCont(s[best])) best++;
    }
    if (best < len && s[best] != ' ' && lastSpace > 0) return lastSpace;
    return best;
}

static const char* skipSpaces(const char* s) {
    while (*s == ' ') s++;
    return s;
}

// Builds the next wrapped line into out; returns pointer past it.
static const char* buildLine(const char* s, int maxPx, bool last, char* out) {
    size_t take = lineTake(s, maxPx);
    bool more = *skipSpaces(s + take) != '\0';
    if (last && more) {
        // Last allowed line and text remains: as much as fits + "...".
        size_t n = strlen(s);
        if (n > TEXT_BUF_MAX - 4) n = TEXT_BUF_MAX - 4;
        while (n > 0) {
            while (n > 0 && isCont(s[n])) n--;
            size_t m = n;
            while (m > 0 && s[m - 1] == ' ') m--;
            memcpy(out, s, m);
            strcpy(out + m, ELLIPSIS);
            if (tft.textWidth(out) <= maxPx) return s + strlen(s);
            if (n == 0) break;
            n--;
        }
        strcpy(out, ELLIPSIS);
        return s + strlen(s);
    }
    size_t m = take;
    while (m > 0 && s[m - 1] == ' ') m--;
    memcpy(out, s, m);
    out[m] = '\0';
    return s + take;
}

int wrapLineCount(const char* src, int maxPx, UiFont f, int maxLines) {
    useFont(f);
    char line[TEXT_BUF_MAX];
    int lines = 0;
    const char* s = skipSpaces(src);
    while (*s && lines < maxLines) {
        s = skipSpaces(buildLine(s, maxPx, lines == maxLines - 1, line));
        lines++;
    }
    return lines;
}

int drawWrapped(const char* src, int x, int y, int maxPx, UiFont f, int lineH,
                int maxLines, uint16_t fg, uint16_t bg, bool strike, bool center) {
    int fh = useFont(f);
    char line[TEXT_BUF_MAX];
    int lines = 0;
    const char* s = skipSpaces(src);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(TL_DATUM);
    while (*s && lines < maxLines) {
        s = skipSpaces(buildLine(s, maxPx, lines == maxLines - 1, line));
        int ly = y + lines * lineH;
        int lw = tft.textWidth(line);
        int lx = center ? x - lw / 2 : x;
        tft.drawString(line, lx, ly);
        if (strike) tft.drawFastHLine(lx, ly + fh / 2 + 1, lw, fg);
        lines++;
    }
    return lines;
}
