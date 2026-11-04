// firmware/src/ui/screen_tasks.cpp
#include "screen_tasks.h"
#include "theme.h"
#include "../display.h"

static const int CARD_H   = 34;
static const int CARD_GAP = 5;
static const int BODY_Y   = HEADER_H + 4;
static const int BODY_END = SCREEN_H - 16;   // leave the page-dots strip
static const int MAX_CARDS_FULL = (BODY_END - BODY_Y + CARD_GAP) / (CARD_H + CARD_GAP);

// Truncate to maxPx wide in font 2, appending "~".
static void fitText(const char* src, char* out, size_t outLen, int maxPx) {
    strlcpy(out, src, outLen);
    if (tft.textWidth(out, 2) <= maxPx) return;
    size_t n = strlen(out);
    while (n > 1) {
        out[--n] = '\0';
        char tmp[72];
        snprintf(tmp, sizeof(tmp), "%s~", out);
        if (tft.textWidth(tmp, 2) <= maxPx) { strlcpy(out, tmp, outLen); return; }
    }
}

static void drawCard(int y, const TaskItem& t) {
    bool done   = strcmp(t.status, "done") == 0;
    bool urgent = !done && strcmp(t.priority, "urgent") == 0;

    const int x = MARGIN, w = SCREEN_W - 2 * MARGIN;
    tft.fillRoundRect(x, y, w, CARD_H, CARD_R, C_CARD);

    uint16_t dotCol  = done ? C_MUTED : (urgent ? C_URGENT : C_OK);
    uint16_t textCol = done ? C_MUTED : C_TEXT;
    tft.fillCircle(x + 14, y + CARD_H / 2, 4, dotCol);

    const int tx = x + 26;
    int chipW = urgent ? 54 : 0;
    int maxPx = w - 26 - 8 - (chipW ? chipW + 6 : 0);

    char txt[72];
    fitText(t.text, txt, sizeof(txt), maxPx);
    tft.setTextColor(textCol, C_CARD);
    tft.drawString(txt, tx, y + 3, 2);
    if (done) {   // strike-through
        int sw = tft.textWidth(txt, 2);
        tft.drawFastHLine(tx, y + 3 + 8, sw, C_MUTED);
    }

    tft.setTextColor(C_MUTED, C_CARD);
    tft.drawString(t.time_display, tx, y + 21, 1);

    if (urgent) {
        int cx = x + w - 8 - chipW;
        tft.fillRoundRect(cx, y + (CARD_H - 16) / 2, chipW, 16, 8, C_URGENT_BG);
        tft.setTextColor(C_URGENT_FG, C_URGENT_BG);
        tft.drawCentreString("urgente", cx + chipW / 2, y + (CARD_H - 16) / 2 + 4, 1);
    }
}

void drawTasksScreen(const TaskItem* tasks, int count) {
    tft.fillRect(0, HEADER_H, SCREEN_W, BODY_END - HEADER_H, C_BG);

    if (count <= 0) {
        tft.setTextColor(C_MUTED, C_BG);
        tft.drawCentreString("Sin tareas", SCREEN_W / 2, SCREEN_H / 2 - 20, 2);
        tft.drawCentreString("Envia texto al bot de Telegram", SCREEN_W / 2, SCREEN_H / 2, 1);
        return;
    }

    // Reserve one row's worth of room for "+N mas" when the list overflows.
    int visible = count;
    if (count > MAX_CARDS_FULL) visible = MAX_CARDS_FULL - 1;

    for (int i = 0; i < visible; i++)
        drawCard(BODY_Y + i * (CARD_H + CARD_GAP), tasks[i]);

    if (count > visible) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d mas", count - visible);
        tft.setTextColor(C_MUTED, C_BG);
        tft.drawCentreString(buf, SCREEN_W / 2, BODY_Y + visible * (CARD_H + CARD_GAP) + 6, 2);
    }
}
