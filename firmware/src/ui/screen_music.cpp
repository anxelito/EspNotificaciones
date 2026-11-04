// firmware/src/ui/screen_music.cpp
#include "screen_music.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "../display.h"

static const int BODY_END  = SCREEN_H - 14;              // page-dots strip
static const int COVER_X   = (SCREEN_W - ARTWORK_SIZE) / 2;
static const int COVER_Y   = HEADER_H + 6;                // as composed by the backend
static const int TEXT_W    = SCREEN_W - 2 * MARGIN - 8;
static const int TITLE_Y   = HEADER_H + 128;              // 154
static const int ARTIST_Y  = TITLE_Y + 28;                // 182
static const int ROW_Y     = BODY_END - 19;               // 207: times + bar
static const int BAR_H     = 4;
static const int BAR_Y     = ROW_Y + 8;
static const int BAR_X     = MARGIN + 44;
static const int BAR_W_PX  = SCREEN_W - 2 * BAR_X;

static const int EQ_BARS = 4, EQ_BAR_W = 3, EQ_GAP = 2, EQ_H = 13;
static const int EQ_W = EQ_BARS * EQ_BAR_W + (EQ_BARS - 1) * EQ_GAP;
static int eqX = -1, eqY = 0;                             // set by drawMusicScreen

uint16_t musicBg(const NowPlaying& np) {
    if (np.hasBg) return np.bg;
    return np.hasAccent ? mix565(np.accent, C_BG, 70) : C_BG;
}

static uint16_t accentLight(const NowPlaying& np) {
    return np.hasAccent ? mix565(np.accent, 0xFFFF, 150) : C_OK;
}

static void fmtMs(int ms, char* out, size_t len) {
    if (ms < 0) ms = 0;
    int s = ms / 1000;
    snprintf(out, len, "%d:%02d", s / 60, s % 60);
}

void drawMusicProgress(const NowPlaying& np, int progressMs) {
    const uint16_t bg = musicBg(np);
    tft.fillRect(0, ROW_Y, SCREEN_W, BODY_END - ROW_Y, bg);
    if (np.duration_ms <= 0) return;                      // no timing: no bar
    if (progressMs > np.duration_ms) progressMs = np.duration_ms;

    tft.fillRoundRect(BAR_X, BAR_Y, BAR_W_PX, BAR_H, 2, mix565(0xFFFF, bg, 45));
    long fillW = (long)BAR_W_PX * progressMs / np.duration_ms;
    uint16_t fc = accentLight(np);
    if (fillW >= BAR_H) tft.fillRoundRect(BAR_X, BAR_Y, (int)fillW, BAR_H, 2, fc);
    else if (fillW > 0)  tft.fillRect(BAR_X, BAR_Y, (int)fillW, BAR_H, fc);
    if (fillW > 0) tft.fillCircle(BAR_X + (int)fillW, BAR_Y + BAR_H / 2, 3, C_TEXT);

    char el[12], tot[12];
    fmtMs(progressMs, el, sizeof(el));
    fmtMs(np.duration_ms, tot, sizeof(tot));
    drawTextRight(el, BAR_X - 8, ROW_Y, UiFont::SMALL, C_TIME, bg);
    drawText(tot, BAR_X + BAR_W_PX + 8, ROW_Y, UiFont::SMALL, C_TIME, bg);
}

void drawMusicEq(const NowPlaying& np, uint32_t now) {
    if (eqX < 0) return;
    const uint16_t bg = musicBg(np), fc = accentLight(np);
    tft.fillRect(eqX, eqY, EQ_W, EQ_H, bg);
    for (int i = 0; i < EQ_BARS; i++) {
        float a = sinf(now / (130.0f + i * 37.0f) + i * 1.7f);
        float b = sinf(now / (71.0f + i * 23.0f) + i);
        int h = 3 + (int)((EQ_H - 3) * (0.5f + 0.3f * a + 0.2f * b));
        if (h > EQ_H) h = EQ_H;
        tft.fillRect(eqX + i * (EQ_BAR_W + EQ_GAP), eqY + EQ_H - h, EQ_BAR_W, h, fc);
    }
}

void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen, int progressMs) {
    const uint16_t bg = musicBg(np);
    if (art && artLen > 0) {
        renderArtwork(art, artLen, 0, HEADER_H);         // composed backdrop
    } else {
        tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, bg);
        tft.fillRoundRect(COVER_X, COVER_Y, ARTWORK_SIZE, ARTWORK_SIZE, 6, mix565(0xFFFF, bg, 30));
        // little note in the placeholder
        int nx = COVER_X + ARTWORK_SIZE / 2 - 8, ny = COVER_Y + ARTWORK_SIZE / 2 - 14;
        uint16_t nc = mix565(0xFFFF, bg, 90);
        tft.fillCircle(nx + 3, ny + 22, 6, nc);
        tft.fillRect(nx + 7, ny, 3, 23, nc);
        tft.fillTriangle(nx + 9, ny, nx + 20, ny + 6, nx + 9, ny + 10, nc);
    }
    // Text area: the image is solid `bg` here; clear it anyway (placeholder
    // case, rounding) so smooth text blends onto exactly that colour.
    tft.fillRect(0, TITLE_Y - 2, SCREEN_W, BODY_END - TITLE_Y + 2, bg);

    const int cx = SCREEN_W / 2;
    drawWrapped(np.title, cx, TITLE_Y, TEXT_W, UiFont::TITLE, 28, 1, C_TEXT, bg, false, true);

    // Artist with the equaliser in front, centred as one group.
    char artist[72];
    fitText(np.artist, artist, sizeof(artist), TEXT_W - EQ_W - 8, UiFont::BODY);
    int aw = textWidthIn(artist, UiFont::BODY);
    int gx = cx - (EQ_W + 8 + aw) / 2;
    eqX = gx;
    eqY = ARTIST_Y + 5;
    drawText(artist, gx + EQ_W + 8, ARTIST_Y, UiFont::BODY, accentLight(np), bg);
    drawMusicEq(np, millis());

    drawMusicProgress(np, progressMs);
}
