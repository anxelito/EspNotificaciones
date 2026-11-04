// firmware/src/ui/screen_music.cpp
#include "screen_music.h"
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "../display.h"

const int MUSIC_ART_X = (SCREEN_W - ARTWORK_SIZE) / 2;   // 100
const int MUSIC_ART_Y = HEADER_H + 3;                     // 29

static const int BODY_END = SCREEN_H - 14;               // page-dots strip
static const int TEXT_W   = SCREEN_W - 2 * MARGIN - 8;
static const int TEXT_Y   = MUSIC_ART_Y + ARTWORK_SIZE + 4;   // 153
static const int TITLE_LH = 28;
static const int ARTIST_LH = 23;

// Bar row: "1:24 [=====     ] 3:31"
static const int TIME_Y   = BODY_END - 19;                // 207
static const int BAR_H    = 4;
static const int BAR_Y    = TIME_Y + 8;
static const int BAR_X    = MARGIN + 44;
static const int BAR_W_PX = SCREEN_W - 2 * BAR_X;

uint16_t musicBg(const NowPlaying& np) {
    return np.hasAccent ? mix565(np.accent, C_BG, 70) : C_BG;
}

static uint16_t accentLight(const NowPlaying& np) {
    return np.hasAccent ? mix565(np.accent, 0xFFFF, 120) : C_OK;
}

static void fmtMs(int ms, char* out, size_t len) {
    if (ms < 0) ms = 0;
    int s = ms / 1000;
    snprintf(out, len, "%d:%02d", s / 60, s % 60);
}

void drawMusicProgress(const NowPlaying& np) {
    const uint16_t bg = musicBg(np);
    tft.fillRect(MARGIN, TIME_Y, SCREEN_W - 2 * MARGIN, BODY_END - TIME_Y, bg);
    if (np.duration_ms <= 0) return;   // source without timing: no bar

    tft.fillRoundRect(BAR_X, BAR_Y, BAR_W_PX, BAR_H, 2, mix565(0xFFFF, bg, 40));
    long fillW = (long)BAR_W_PX * np.progress_ms / np.duration_ms;
    if (fillW > BAR_W_PX) fillW = BAR_W_PX;
    uint16_t fc = accentLight(np);
    if (fillW >= BAR_H) tft.fillRoundRect(BAR_X, BAR_Y, (int)fillW, BAR_H, 2, fc);
    else if (fillW > 0)  tft.fillRect(BAR_X, BAR_Y, (int)fillW, BAR_H, fc);

    char el[12], tot[12];
    fmtMs(np.progress_ms, el, sizeof(el));
    fmtMs(np.duration_ms, tot, sizeof(tot));
    drawTextRight(el, BAR_X - 6, TIME_Y, UiFont::SMALL, C_MUTED, bg);
    drawText(tot, BAR_X + BAR_W_PX + 6, TIME_Y, UiFont::SMALL, C_MUTED, bg);
}

void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen) {
    const uint16_t bg = musicBg(np);
    tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, bg);

    // Cover with a soft frame; placeholder until the JPEG is decoded over it.
    tft.fillRoundRect(MUSIC_ART_X - 4, MUSIC_ART_Y - 2, ARTWORK_SIZE + 8, ARTWORK_SIZE + 4,
                      CARD_R, mix565(0xFFFF, bg, 28));
    tft.fillRect(MUSIC_ART_X, MUSIC_ART_Y, ARTWORK_SIZE, ARTWORK_SIZE, mix565(0xFFFF, bg, 18));
    if (art && artLen > 0) renderArtwork(art, artLen, MUSIC_ART_X, MUSIC_ART_Y);

    const bool hasBar = np.duration_ms > 0;
    const int cx = SCREEN_W / 2;
    const int bottom = hasBar ? TIME_Y : BODY_END - 2;
    const bool hasArtist = np.artist[0] != '\0';
    // Two title lines only if the artist still fits below them.
    int titleLines = (TEXT_Y + 2 * TITLE_LH + (hasArtist ? ARTIST_LH : 0) <= bottom) ? 2 : 1;
    int y = TEXT_Y;
    y += drawWrapped(np.title, cx, y, TEXT_W, UiFont::TITLE, TITLE_LH, titleLines,
                     C_TEXT, bg, false, true) * TITLE_LH;
    if (hasArtist && y + ARTIST_LH <= bottom + 2)
        drawWrapped(np.artist, cx, y, TEXT_W, UiFont::BODY, ARTIST_LH, 1,
                    accentLight(np), bg, false, true);

    drawMusicProgress(np);
}
