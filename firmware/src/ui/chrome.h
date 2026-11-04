// firmware/src/ui/chrome.h -- header bar, toast banner and page dots
#pragma once
#include <stdint.h>
#include "../logic/room.h"

struct HeaderInfo {
    const char* title;          // UTF-8, e.g. "TAREAS · 3/5"
    bool online;
    bool timeKnown;
    int  hour, minute;
    bool playing;
    const RoomReading* room;    // may be null
    bool roomFresh;
    uint16_t bg;                // background (music screen tints it)
    bool showTime;              // false on Habitacion (it has the big clock)
};

// Draws into its own rect (0,0,SCREEN_W,HEADER_H); no full-screen clear.
void drawHeader(const HeaderInfo& h);
// Notification banner over the header rect (e.g. "Nueva tarea" + text).
void drawToast(const char* label, const char* text, uint16_t accent);
// Draws into the bottom strip; index 0-based.
void drawPageDots(int index, int count, uint16_t bg);
