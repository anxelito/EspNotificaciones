// firmware/src/ui/screen_tasks.cpp
#include "screen_tasks.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "../display.h"

// Text in FONT_BODY (wrapped, up to TEXT_LINES lines), time below in FONT_SMALL.
static const int TEXT_LINE  = 22;
static const int TEXT_LINES = 2;
static const int PAD_T      = 5;
static const int TIME_H     = 18;
static const int CARD_GAP   = 5;
static const int BAR_Y      = HEADER_H + 1;    // progress bar under the header
static const int BAR_H      = 3;
static const int BODY_Y     = HEADER_H + 7;
static const int BODY_END   = SCREEN_H - 16;   // leave the page-dots strip
static const int MORE_H     = 20;              // "+N más" row
static const int CHIP_H     = 18;
static const char CHIP_TXT[] = "urgente";

static const int CARD_X  = MARGIN;
static const int CARD_W  = SCREEN_W - 2 * MARGIN;
static const int TEXT_X  = CARD_X + 26;
static const int DOT_X   = CARD_X + 14;

// Urgent dots on screen, for drawTasksPulse.
static int pulseY[MAX_TASKS];
static int pulseN = 0;

static int chipW() { return textWidthIn(CHIP_TXT, UiFont::SMALL) + 14; }

static int textMaxPx(bool urgent) {
    return CARD_W - 26 - 10 - (urgent ? chipW() + 6 : 0);
}

static int cardHeight(const TaskItem& t, bool urgent) {
    int lines = wrapLineCount(t.text, textMaxPx(urgent), UiFont::BODY, TEXT_LINES);
    if (lines < 1) lines = 1;
    return PAD_T + lines * TEXT_LINE + TIME_H + 4;
}

static void drawCard(int y, int h, const TaskItem& t, bool done, bool urgent) {
    tft.fillRoundRect(CARD_X, y, CARD_W, h, CARD_R, C_CARD);

    uint16_t dotCol  = done ? C_MUTED : (urgent ? C_URGENT : C_OK);
    uint16_t textCol = done ? C_MUTED : C_TEXT;
    tft.fillCircle(DOT_X, y + h / 2, 5, dotCol);
    if (urgent && pulseN < MAX_TASKS) pulseY[pulseN++] = y + h / 2;

    int lines = drawWrapped(t.text, TEXT_X, y + PAD_T, textMaxPx(urgent), UiFont::BODY,
                            TEXT_LINE, TEXT_LINES, textCol, C_CARD, done);
    if (lines < 1) lines = 1;
    drawText(t.time_display, TEXT_X, y + PAD_T + lines * TEXT_LINE, UiFont::SMALL,
             C_MUTED, C_CARD);

    if (urgent) {
        int w = chipW();
        int cx = CARD_X + CARD_W - 8 - w;
        int cy = y + 6;
        tft.fillRoundRect(cx, cy, w, CHIP_H, CHIP_H / 2, C_URGENT_BG);
        drawTextCentre(CHIP_TXT, cx + w / 2, cy, UiFont::SMALL, C_URGENT_FG, C_URGENT_BG);
    }
}

static void drawProgress(int done, int total) {
    tft.fillRect(0, BAR_Y, SCREEN_W, BAR_H, C_BG);
    if (total <= 0) return;
    int w = SCREEN_W - 2 * MARGIN;
    tft.fillRoundRect(MARGIN, BAR_Y, w, BAR_H, 1, C_DOT_OFF);
    int fw = w * done / total;
    if (fw > 0) tft.fillRoundRect(MARGIN, BAR_Y, fw, BAR_H, 1, C_OK);
}

void drawTasksScreen(const TaskItem* tasks, int count) {
    tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, C_BG);
    pulseN = 0;

    int done = 0;
    for (int i = 0; i < count; i++) if (strcmp(tasks[i].status, "done") == 0) done++;
    drawProgress(done, count);

    if (count <= 0) {
        drawTextCentre("Sin tareas", SCREEN_W / 2, SCREEN_H / 2 - 26, UiFont::BODY, C_TEXT, C_BG);
        drawTextCentre("Envía un mensaje al bot de Telegram", SCREEN_W / 2, SCREEN_H / 2,
                       UiFont::SMALL, C_MUTED, C_BG);
        return;
    }

    int y = BODY_Y, shown = 0;
    for (int i = 0; i < count; i++) {
        bool isDone = strcmp(tasks[i].status, "done") == 0;
        bool urgent = !isDone && strcmp(tasks[i].priority, "urgent") == 0;
        int h = cardHeight(tasks[i], urgent);
        bool lastOne = (i == count - 1);
        // Keep room for "+N más" unless this is the final task.
        int limit = BODY_END - (lastOne ? 0 : MORE_H);
        if (y + h > limit) break;
        drawCard(y, h, tasks[i], isDone, urgent);
        y += h + CARD_GAP;
        shown++;
    }

    if (shown < count) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d más", count - shown);
        drawTextCentre(buf, SCREEN_W / 2, y, UiFont::SMALL, C_MUTED, C_BG);
    }
}

void drawTasksPulse(uint32_t nowMs) {
    if (pulseN == 0) return;
    float s = 0.5f + 0.5f * sinf((float)(nowMs % 1400) * (2.0f * 3.14159265f / 1400.0f));
    uint16_t col = mix565(C_URGENT, C_URGENT_BG, (uint8_t)(110 + 145 * s));
    int r = 4 + (s > 0.5f ? 1 : 0);
    for (int i = 0; i < pulseN; i++) {
        tft.fillCircle(DOT_X, pulseY[i], 6, C_CARD);
        tft.fillCircle(DOT_X, pulseY[i], r, col);
    }
}
