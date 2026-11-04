// firmware/src/ui/chrome.h -- header bar and page dots
#pragma once
#include "../logic/room.h"

// TFT fonts are ASCII only: "\x01" inside a title is drawn as a middle dot
// (e.g. "TAREAS \x01 3").
#define TITLE_DOT "\x01"

struct HeaderInfo {
    const char* title;
    bool online;
    bool timeKnown;
    int  hour, minute;
    bool playing;
    const RoomReading* room;   // may be null
    bool roomFresh;
};

// Draws into its own rect (0,0,SCREEN_W,HEADER_H); no full-screen clear.
void drawHeader(const HeaderInfo& h);
// Draws into the bottom strip; index 0-based.
void drawPageDots(int index, int count);
