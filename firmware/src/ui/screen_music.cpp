// firmware/src/ui/screen_music.cpp
#include "screen_music.h"
#include "theme.h"
#include "../display.h"
#include "text_fit.h"

const int MUSIC_ART_X = MARGIN + 12;
const int MUSIC_ART_Y = HEADER_H + 8 + 12;

static const int CARD_X = MARGIN;
static const int CARD_Y = HEADER_H + 8;
static const int CARD_W = SCREEN_W - 2 * MARGIN;
static const int CARD_H = 160;

static const int TEXT_X = MUSIC_ART_X + ARTWORK_SIZE + 12;
static const int TEXT_W = CARD_X + CARD_W - 10 - TEXT_X;

static const int BAR_X = MARGIN + 12;
static const int BAR_W_PX = CARD_W - 24;
static const int BAR_H = 5;
static const int BAR_Y = CARD_Y + 116;
static const int TIME_Y = BAR_Y + BAR_H + 6;

static void fmtMs(int ms, char* out, size_t len) {
    if (ms < 0) ms = 0;
    int s = ms / 1000;
    snprintf(out, len, "%d:%02d", s / 60, s % 60);
}

void drawMusicProgress(const NowPlaying& np) {
    if (np.duration_ms <= 0) {
        // Source without timing (PC agent, Spotify free): no bar, no times.
        tft.fillRect(BAR_X, BAR_Y, BAR_W_PX, TIME_Y + 16 - BAR_Y, C_CARD);
        return;
    }
    tft.fillRoundRect(BAR_X, BAR_Y, BAR_W_PX, BAR_H, 2, C_DOT_OFF);
    if (np.duration_ms > 0) {
        long fillW = (long)BAR_W_PX * np.progress_ms / np.duration_ms;
        if (fillW > BAR_W_PX) fillW = BAR_W_PX;
        if (fillW >= BAR_H) tft.fillRoundRect(BAR_X, BAR_Y, (int)fillW, BAR_H, 2, C_OK);
        else if (fillW > 0)  tft.fillRect(BAR_X, BAR_Y, (int)fillW, BAR_H, C_OK);
    }

    char el[12], tot[12];
    fmtMs(np.progress_ms, el, sizeof(el));
    fmtMs(np.duration_ms, tot, sizeof(tot));
    tft.setTextColor(C_MUTED, C_CARD);
    tft.setTextPadding(48);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(el, BAR_X, TIME_Y, 2);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(tot, BAR_X + BAR_W_PX, TIME_Y, 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen) {
    // Clear the body (header and page dots are drawn by the caller).
    tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H - 16, C_BG);
    tft.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, CARD_R, C_CARD);

    // Artwork placeholder, replaced by the decoded JPEG when available.
    tft.fillRect(MUSIC_ART_X, MUSIC_ART_Y, ARTWORK_SIZE, ARTWORK_SIZE, C_DOT_OFF);
    if (art && artLen > 0) renderArtwork(art, artLen, MUSIC_ART_X, MUSIC_ART_Y);

    char buf[72];
    fitText(np.title, buf, sizeof(buf), TEXT_W, 4);
    tft.setTextColor(C_TEXT, C_CARD);
    tft.drawString(buf, TEXT_X, MUSIC_ART_Y + 2, 4);

    fitText(np.artist, buf, sizeof(buf), TEXT_W, 2);
    tft.setTextColor(C_OK, C_CARD);
    tft.drawString(buf, TEXT_X, MUSIC_ART_Y + 32, 2);

    fitText(np.album, buf, sizeof(buf), TEXT_W, 2);
    tft.setTextColor(C_MUTED, C_CARD);
    tft.drawString(buf, TEXT_X, MUSIC_ART_Y + 50, 2);

    drawMusicProgress(np);
}
