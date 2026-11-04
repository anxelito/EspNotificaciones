// firmware/src/ui/screen_music.cpp -- Now Playing: big centred cover, then
// title and artist centred below, progress bar at the bottom when known.
#include "screen_music.h"
#include "theme.h"
#include "../display.h"
#include "text_fit.h"

const int MUSIC_ART_X = (SCREEN_W - ARTWORK_SIZE) / 2;   // 100
const int MUSIC_ART_Y = HEADER_H + 4;                     // 30

static const int BODY_END = SCREEN_H - 16;               // page-dots strip
static const int TEXT_W   = SCREEN_W - 2 * MARGIN - 8;
static const int TEXT_Y   = MUSIC_ART_Y + ARTWORK_SIZE + 5;   // 155
static const int LINE4    = 22;

// Bar row: "1:24 [=====     ] 3:31"
static const int TIME_Y   = BODY_END - 18;                // 206
static const int BAR_H    = 4;
static const int BAR_Y    = TIME_Y + 6;
static const int BAR_X    = MARGIN + 44;
static const int BAR_W_PX = SCREEN_W - 2 * BAR_X;

static void fmtMs(int ms, char* out, size_t len) {
    if (ms < 0) ms = 0;
    int s = ms / 1000;
    snprintf(out, len, "%d:%02d", s / 60, s % 60);
}

void drawMusicProgress(const NowPlaying& np) {
    // Clear the whole bar row (times included).
    tft.fillRect(MARGIN, TIME_Y, SCREEN_W - 2 * MARGIN, 16, C_BG);
    if (np.duration_ms <= 0) return;   // source without timing: no bar

    tft.fillRoundRect(BAR_X, BAR_Y, BAR_W_PX, BAR_H, 2, C_DOT_OFF);
    long fillW = (long)BAR_W_PX * np.progress_ms / np.duration_ms;
    if (fillW > BAR_W_PX) fillW = BAR_W_PX;
    if (fillW >= BAR_H) tft.fillRoundRect(BAR_X, BAR_Y, (int)fillW, BAR_H, 2, C_OK);
    else if (fillW > 0)  tft.fillRect(BAR_X, BAR_Y, (int)fillW, BAR_H, C_OK);

    char el[12], tot[12];
    fmtMs(np.progress_ms, el, sizeof(el));
    fmtMs(np.duration_ms, tot, sizeof(tot));
    tft.setTextColor(C_MUTED, C_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(el, BAR_X - 6, TIME_Y, 2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(tot, BAR_X + BAR_W_PX + 6, TIME_Y, 2);
}

void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen) {
    // Clear the body (header and page dots are drawn by the caller).
    tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, C_BG);

    // Cover: soft card frame, placeholder until the JPEG is decoded over it.
    tft.fillRoundRect(MUSIC_ART_X - 4, MUSIC_ART_Y - 2, ARTWORK_SIZE + 8, ARTWORK_SIZE + 4,
                      CARD_R, C_CARD);
    tft.fillRect(MUSIC_ART_X, MUSIC_ART_Y, ARTWORK_SIZE, ARTWORK_SIZE, C_DOT_OFF);
    if (art && artLen > 0) renderArtwork(art, artLen, MUSIC_ART_X, MUSIC_ART_Y);

    // Title (font 4; 2 lines when there is no bar), artist below in green.
    const bool hasBar = np.duration_ms > 0;
    const int cx = SCREEN_W / 2;
    int titleLines = hasBar ? 1 : 2;
    int artistBottom = hasBar ? TIME_Y - 2 : BODY_END - 2;
    // Leave room for the artist line: only use 2 title lines if it still fits.
    if (titleLines == 2 && TEXT_Y + 2 * LINE4 + 17 > artistBottom) titleLines = 1;
    int y = TEXT_Y;
    y += drawWrapped(np.title, cx, y, TEXT_W, 4, LINE4, titleLines, C_TEXT, C_BG,
                     false, true) * LINE4 + 1;
    if (np.artist[0] && y + 16 <= artistBottom)
        drawWrapped(np.artist, cx, y, TEXT_W, 2, 17, 1, C_OK, C_BG, false, true);

    drawMusicProgress(np);
}
