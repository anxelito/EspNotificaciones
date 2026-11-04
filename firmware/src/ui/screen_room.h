// firmware/src/ui/screen_room.h -- Habitacion screen: cat + TEMP/HUMEDAD/CONFORT strip
#pragma once
#include "../config.h"
#include "../logic/room.h"
#include "cat_draw.h"

// Layout: header 0..26, room scene 26..184 (window, clock, cat, floor; see
// room_scene.h), strip 184..226, page dots 226..240.
#define ROOM_STRIP_Y   184
#define ROOM_STRIP_END (SCREEN_H - 14)                        // page dots start
#define ROOM_CAT_X     ((SCREEN_W - CAT_W) / 2)               // 110
#define ROOM_CAT_Y     88                                     // below the clock info line

// Bottom strip; repaints only the columns whose text/colour changed.
void drawRoomStrip(const RoomReading& r, bool fresh);
// Paints the static room (wall, floor, rug, plant, window sill) once on entry
// and forces the next drawRoomStrip to repaint everything.
void drawRoomScreen();
