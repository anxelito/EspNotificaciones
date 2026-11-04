// firmware/src/ui/screen_room.h -- Habitacion screen: cat + TEMP/HUMEDAD/CONFORT strip
#pragma once
#include "../config.h"
#include "../logic/room.h"
#include "cat_draw.h"

// Layout: header 0..26, cat area 26..184, strip 184..226, page dots 226..240
// (the 56 px bottom band = strip + the shared page-dots row).
#define ROOM_STRIP_Y   184
#define ROOM_STRIP_END (SCREEN_H - 14)                        // page dots start
#define ROOM_CAT_X     ((SCREEN_W - CAT_W) / 2)               // 110
#define ROOM_CAT_Y     (HEADER_H + (ROOM_STRIP_Y - HEADER_H - CAT_H) / 2)  // 60

// Bottom strip; repaints only the columns whose text/colour changed.
void drawRoomStrip(const RoomReading& r, bool fresh);
// Clears the body (header..page dots) once on entry and forces the next
// drawRoomStrip to repaint everything.
void drawRoomScreen();
