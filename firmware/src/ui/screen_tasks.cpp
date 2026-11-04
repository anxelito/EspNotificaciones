// firmware/src/ui/screen_tasks.cpp
#include "screen_tasks.h"
#include "theme.h"
#include "../display.h"
#include "text_fit.h"

// Text in font 4 (wrapped, up to TEXT_LINES lines), time below in font 2.
static const int TEXT_FONT  = 4;
static const int TEXT_LINE  = 22;   // px per wrapped line of font 4
static const int TEXT_LINES = 2;
static const int PAD_T      = 5;
static const int TIME_H     = 16;   // font 2
static const int CARD_GAP   = 5;
static const int BODY_Y     = HEADER_H + 4;
static const int BODY_END   = SCREEN_H - 16;   // leave the page-dots strip
static const int MORE_H     = 20;              // "+N mas" row
static const int CHIP_W     = 62, CHIP_H = 18;

static const int CARD_X  = MARGIN;
static const int CARD_W  = SCREEN_W - 2 * MARGIN;
static const int TEXT_X  = CARD_X + 26;

static int textMaxPx(bool urgent) {
    return CARD_W - 26 - 8 - (urgent ? CHIP_W + 6 : 0);
}

static int cardHeight(const TaskItem& t, bool urgent) {
    int lines = wrapLineCount(t.text, textMaxPx(urgent), TEXT_FONT, TEXT_LINES);
    if (lines < 1) lines = 1;
    return PAD_T + lines * TEXT_LINE + 1 + TIME_H + 3;
}

static void drawCard(int y, int h, const TaskItem& t, bool done, bool urgent) {
    tft.fillRoundRect(CARD_X, y, CARD_W, h, CARD_R, C_CARD);

    uint16_t dotCol  = done ? C_MUTED : (urgent ? C_URGENT : C_OK);
    uint16_t textCol = done ? C_MUTED : C_TEXT;
    tft.fillCircle(CARD_X + 14, y + h / 2, 5, dotCol);

    int lines = drawWrapped(t.text, TEXT_X, y + PAD_T, textMaxPx(urgent), TEXT_FONT,
                            TEXT_LINE, TEXT_LINES, textCol, C_CARD, done);
    if (lines < 1) lines = 1;

    tft.setTextColor(C_MUTED, C_CARD);
    tft.drawString(t.time_display, TEXT_X, y + PAD_T + lines * TEXT_LINE + 1, 2);

    if (urgent) {
        int cx = CARD_X + CARD_W - 8 - CHIP_W;
        int cy = y + 7;
        tft.fillRoundRect(cx, cy, CHIP_W, CHIP_H, CHIP_H / 2, C_URGENT_BG);
        tft.setTextColor(C_URGENT_FG, C_URGENT_BG);
        tft.drawCentreString("urgente", cx + CHIP_W / 2, cy + 1, 2);
    }
}

void drawTasksScreen(const TaskItem* tasks, int count) {
    tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, C_BG);

    if (count <= 0) {
        tft.setTextColor(C_MUTED, C_BG);
        tft.drawCentreString("Sin tareas", SCREEN_W / 2, SCREEN_H / 2 - 20, 2);
        tft.drawCentreString("Envia texto al bot de Telegram", SCREEN_W / 2, SCREEN_H / 2, 2);
        return;
    }

    int y = BODY_Y, shown = 0;
    for (int i = 0; i < count; i++) {
        bool done   = strcmp(tasks[i].status, "done") == 0;
        bool urgent = !done && strcmp(tasks[i].priority, "urgent") == 0;
        int h = cardHeight(tasks[i], urgent);
        bool lastOne = (i == count - 1);
        // Keep room for "+N mas" unless this is the final task.
        int limit = BODY_END - (lastOne ? 0 : MORE_H);
        if (y + h > limit) break;
        drawCard(y, h, tasks[i], done, urgent);
        y += h + CARD_GAP;
        shown++;
    }

    if (shown < count) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d mas", count - shown);
        tft.setTextColor(C_MUTED, C_BG);
        tft.drawCentreString(buf, SCREEN_W / 2, y + 1, 2);
    }
}
