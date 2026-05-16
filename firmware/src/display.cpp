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
    tft.drawString("TAREAS", 8, 8, 2);

    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawCentreString(online ? time_str : "OFFLINE", SCREEN_W / 2, 8, 2);

    tft.setTextColor(TFT_DARKGREY, C_HEADER);
    tft.drawRightString(String(pending) + " pend", SCREEN_W - 6, 8, 2);
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

    // FreeSansBold18pt7b ~36px, ~16px per char, usable ~255px -> ~15 chars
    String txt = String(task.text);
    if (txt.length() > 15) txt = txt.substring(0, 14) + "~";

    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.setTextColor(textColor, cardBg);
    tft.drawString(txt, BAR_W + 6, y + 6);
    tft.setTextFont(2);  // back to built-in font 2

    tft.setTextColor(metaColor, cardBg);
    tft.drawString(String(task.time_display), BAR_W + 6, y + 46, 2);

    const char* badge = done ? "DONE" : (urgent ? "URGENTE" : "PEND");
    tft.setTextColor(barColor, cardBg);
    tft.drawRightString(badge, SCREEN_W - 6, y + 46, 2);
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

void drawCalendarPanel(CalEvent* events, int count, const String& time_str, bool online) {
    tft.fillScreen(C_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("CALENDARIO", 8, 8, 2);
    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawRightString(online ? time_str : "OFFLINE", SCREEN_W - 6, 8, 2);

    if (count == 0) {
        tft.setTextColor(TFT_DARKGREY, C_BG);
        tft.drawCentreString("Sin eventos", SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
        return;
    }

    const int CAL_H = 42;
    const int SEC_H = 20;

    int y      = HEADER_H + TASK_MARGIN;
    int8_t lastDay = -1;

    for (int i = 0; i < count; i++) {
        CalEvent& e = events[i];

        if ((int8_t)e.day != lastDay) {
            if (y + SEC_H > SCREEN_H - 4) break;
            tft.fillRect(0, y, SCREEN_W, SEC_H, C_HEADER);
            tft.setTextColor(TFT_CYAN, C_HEADER);
            tft.drawString(e.day == 0 ? "HOY" : "MANANA", BAR_W + 6, y + 3, 2);
            y += SEC_H + 2;
            lastDay = e.day;
        }

        if (y + CAL_H > SCREEN_H - 2) break;

        uint16_t barColor = (e.day == 0) ? TFT_CYAN : TFT_YELLOW;

        tft.fillRect(0, y, SCREEN_W, CAL_H, C_CARD);
        tft.fillRect(0, y, BAR_W, CAL_H, barColor);

        String txt = String(e.summary);
        if (txt.length() > 22) txt = txt.substring(0, 21) + "~";
        tft.setTextColor(TFT_WHITE, C_CARD);
        tft.drawString(txt, BAR_W + 6, y + 6, 2);

        tft.setTextColor(barColor, C_CARD);
        tft.drawRightString(String(e.time_display), SCREEN_W - 6, y + 24, 2);

        y += CAL_H + TASK_MARGIN;
    }
}

void drawWeatherPanel(const WeatherData& weather, const String& time_str, bool online) {
    tft.fillScreen(C_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("TIEMPO", 8, 8, 2);
    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawRightString(online ? time_str : "OFFLINE", SCREEN_W - 6, 8, 2);

    // Clock card
    const int CLOCK_H = 76;
    int y = HEADER_H + TASK_MARGIN;
    tft.fillRect(0, y, SCREEN_W, CLOCK_H, C_CARD);
    tft.fillRect(0, y, BAR_W, CLOCK_H, TFT_CYAN);
    tft.setTextColor(TFT_WHITE, C_CARD);
    tft.drawCentreString(time_str, SCREEN_W / 2, y + 12, 6);

    // Weather card — fills remaining space
    y += CLOCK_H + TASK_MARGIN;
    const int WX_H = SCREEN_H - y - TASK_MARGIN;

    uint16_t condColor = TFT_DARKGREY;
    if (strstr(weather.description, "Despejado"))  condColor = TFT_YELLOW;
    else if (strstr(weather.description, "Nubes")) condColor = 0xC618;  // light grey
    else if (strstr(weather.description, "Lluvia") ||
             strstr(weather.description, "Llovizna") ||
             strstr(weather.description, "Chubascos")) condColor = TFT_BLUE;
    else if (strstr(weather.description, "Tormenta")) condColor = TFT_RED;
    else if (strstr(weather.description, "Nieve"))    condColor = TFT_WHITE;

    tft.fillRect(0, y, SCREEN_W, WX_H, C_CARD);
    tft.fillRect(0, y, BAR_W, WX_H, condColor);

    String tempStr = String(weather.temp, 1) + "C";
    tft.setTextColor(TFT_CYAN, C_CARD);
    tft.drawCentreString(tempStr, SCREEN_W / 2, y + 10, 4);

    tft.setTextColor(TFT_DARKGREY, C_CARD);
    tft.drawCentreString(weather.description, SCREEN_W / 2, y + 46, 2);
}

void drawPanel(int panel, TaskItem* tasks, int taskCount,
               CalEvent* events, int calCount,
               const WeatherData& weather,
               const String& time_str, bool online) {
    switch (panel) {
        case 0: redrawAll(tasks, taskCount, time_str, online); break;
        case 1: drawCalendarPanel(events, calCount, time_str, online); break;
        case 2: drawWeatherPanel(weather, time_str, online);   break;
    }
}
