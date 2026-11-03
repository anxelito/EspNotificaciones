// firmware/src/display.cpp
#include "display.h"

TFT_eSPI tft = TFT_eSPI();

// Colors
#define C_BG       0x0000  // black
#define C_HEADER   0x2104  // dark grey
#define C_CARD     0x0841  // very dark grey
#define C_DONE_BG  0x0421  // near-black for done cards

void displayInit() {
    tft.init();
    tft.setRotation(1);  // landscape
    tft.fillScreen(C_BG);
}

void drawSplash(const String& msg) {
    tft.fillScreen(C_BG);
    tft.setTextColor(TFT_CYAN, C_BG);
    tft.drawCentreString("ESP32 TASKS", SCREEN_W / 2, 90, 4);
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString(msg, SCREEN_W / 2, 130, 2);
}

static void drawHeader(const String& time_str, int pending, bool online) {
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);

    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("TAREAS", 8, 7, 2);

    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawCentreString(online ? time_str : "OFFLINE", SCREEN_W / 2, 7, 2);

    tft.setTextColor(TFT_DARKGREY, C_HEADER);
    tft.drawRightString(String(pending) + " pend", SCREEN_W - 6, 7, 2);
}

static void drawTask(int index, const TaskItem& task) {
    int y = HEADER_H + TASK_MARGIN + index * (TASK_H + TASK_MARGIN);
    if (y + TASK_H > SCREEN_H - 10) return;

    bool done   = strcmp(task.status, "done") == 0;
    bool urgent = strcmp(task.priority, "urgent") == 0;

    uint16_t barColor  = done ? TFT_DARKGREY : (urgent ? TFT_RED : TFT_GREEN);
    uint16_t cardBg    = done ? C_DONE_BG : C_CARD;
    uint16_t textColor = done ? TFT_DARKGREY : TFT_WHITE;
    uint16_t metaColor = done ? 0x2945 : 0x4228;

    tft.fillRect(0, y, SCREEN_W, TASK_H, cardBg);
    tft.fillRect(0, y, BAR_W, TASK_H, barColor);

    // Truncate text to fit (font 2 ~12px per char, usable width ~260px -> ~26 chars)
    String txt = String(task.text);
    if (txt.length() > 26) txt = txt.substring(0, 25) + "~";

    tft.setTextColor(textColor, cardBg);
    tft.drawString(txt, BAR_W + 6, y + 5, 2);

    tft.setTextColor(metaColor, cardBg);
    tft.drawString(String(task.time_display), BAR_W + 6, y + 22, 1);

    const char* badge = done ? "DONE" : (urgent ? "URGENTE" : "PEND");
    tft.setTextColor(barColor, cardBg);
    tft.drawRightString(badge, SCREEN_W - 6, y + 13, 1);
}

static void drawNoTasks() {
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString("Sin tareas", SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
    tft.drawCentreString("Envia texto al bot de Telegram", SCREEN_W / 2, SCREEN_H / 2 + 10, 1);
}

void redrawAll(TaskItem* tasks, int count, const String& time_str, bool online) {
    int pending = 0;
    for (int i = 0; i < count; i++)
        if (strcmp(tasks[i].status, "pending") == 0) pending++;

    drawHeader(time_str, pending, online);
    tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, C_BG);

    if (count == 0) {
        drawNoTasks();
        return;
    }

    int maxVisible = (SCREEN_H - HEADER_H - TASK_MARGIN) / (TASK_H + TASK_MARGIN);
    int visible    = min(count, maxVisible);

    for (int i = 0; i < visible; i++) {
        drawTask(i, tasks[i]);
    }

    if (count > maxVisible) {
        tft.setTextColor(TFT_DARKGREY, C_BG);
        tft.drawCentreString(
            "+" + String(count - maxVisible) + " mas",
            SCREEN_W / 2, SCREEN_H - 10, 1
        );
    }
}

// --- Now Playing ---

// TJpg_Decoder callback: push each decoded MCU block to the TFT
static bool _jpgRenderCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    tft.pushImage(x, y, w, h, bitmap);
    return true;  // continue decoding
}

void renderArtwork(const uint8_t* jpegBuf, int jpegLen, int x, int y) {
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(_jpgRenderCallback);
    TJpgDec.setJpgScale(1);  // 1:1 scale — image is already 64x64
    TJpgDec.drawJpg(x, y, jpegBuf, jpegLen);
}

static String formatMs(int ms) {
    int totalSec = ms / 1000;
    int m = totalSec / 60;
    int s = totalSec % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return String(buf);
}

// Now Playing colors
#define C_NP_TITLE   TFT_WHITE
#define C_NP_ARTIST  TFT_CYAN
#define C_NP_ALBUM   TFT_DARKGREY
#define C_NP_BAR_BG  0x2104   // dark grey
#define C_NP_BAR_FG  TFT_GREEN

void drawNowPlaying(const NowPlaying& np, const String& time_str) {
    // --- Header ---
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    tft.setTextColor(TFT_GREEN, C_HEADER);
    tft.drawString("\x0E NOW PLAYING", 8, 7, 2);  // ♫ symbol or fallback
    tft.setTextColor(TFT_GREEN, C_HEADER);
    tft.drawRightString(time_str, SCREEN_W - 6, 7, 2);

    // --- Body background ---
    tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, C_BG);

    // Layout constants
    const int artX  = 20;
    const int artY  = HEADER_H + 30;
    const int artSz = ARTWORK_SIZE;  // 64
    const int textX = artX + artSz + 16;
    const int textW = SCREEN_W - textX - 10;

    // Artwork placeholder (grey square) — actual art rendered via renderArtwork()
    tft.fillRect(artX, artY, artSz, artSz, 0x2104);
    // Draw a thin border around artwork area
    tft.drawRect(artX - 1, artY - 1, artSz + 2, artSz + 2, 0x4228);

    // --- Song info (right of artwork, vertically centered) ---
    int infoY = artY + 4;

    // Title (truncate to fit)
    String title = String(np.title);
    if (title.length() > 18) title = title.substring(0, 17) + "~";
    tft.setTextColor(C_NP_TITLE, C_BG);
    tft.drawString(title, textX, infoY, 4);

    // Artist
    String artist = String(np.artist);
    if (artist.length() > 24) artist = artist.substring(0, 23) + "~";
    tft.setTextColor(C_NP_ARTIST, C_BG);
    tft.drawString(artist, textX, infoY + 30, 2);

    // Album
    String album = String(np.album);
    if (album.length() > 28) album = album.substring(0, 27) + "~";
    tft.setTextColor(C_NP_ALBUM, C_BG);
    tft.drawString(album, textX, infoY + 50, 2);

    // --- Progress bar ---
    int barY = artY + artSz + 20;
    int barX = 20;
    int barW = SCREEN_W - 40;
    int barH = 6;

    // Background
    tft.fillRoundRect(barX, barY, barW, barH, 3, C_NP_BAR_BG);

    // Fill based on progress
    if (np.duration_ms > 0) {
        int fillW = (int)((long)barW * np.progress_ms / np.duration_ms);
        if (fillW > barW) fillW = barW;
        if (fillW > 0) {
            tft.fillRoundRect(barX, barY, fillW, barH, 3, C_NP_BAR_FG);
        }
    }

    // Time labels
    String elapsed = formatMs(np.progress_ms);
    String total   = formatMs(np.duration_ms);
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawString(elapsed, barX, barY + barH + 4, 1);
    tft.drawRightString(total, barX + barW, barY + barH + 4, 1);
}
