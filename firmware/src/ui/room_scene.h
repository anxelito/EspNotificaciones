// firmware/src/ui/room_scene.h -- the cat's room: window (sky by hour + live
// weather), big clock, floor with rug and a plant.
//
// Layout (Habitacion body 26..184):
//   window   x 12..80,  y 30..82   own 68x52 sprite, animated
//   clock    centred at x ROOM_CLOCK_CX, y 30..64 (FONT_CLOCK)
//   info     "mié 4 nov · 13° Nublado" (shortened to fit), y 66..85
//   cat      sprite y ROOM_CAT_Y..+90, roams x ROOM_BOT_MIN_X..ROOM_BOT_MAX_X
//   floor    y ROOM_FLOOR_Y..ROOM_STRIP_Y, rug + plant
#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>
#include "../logic/room.h"

#define ROOM_FLOOR_Y   170
#define ROOM_CLOCK_CX  199
#define ROOM_BOT_MIN_X 8
#define ROOM_BOT_MAX_X 186     // sprite right edge stays left of the plant

enum class Sky : uint8_t { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, STORM };

struct SceneInfo {
    bool  timeKnown;
    int   hour, minute;
    int   wday, mday, mon;   // date (valid when timeKnown)
    bool  weatherOk;
    int   weatherCode;       // WMO code from Open-Meteo
    float outTemp;           // °C outside
    bool  night;             // (unused here: the window sky follows the hour)
    Comfort comfort;
};

Sky skyFromWmo(int code);

// Creates the window sprite (~7 KB). Safe to call more than once.
void sceneInit();
// Kept for API symmetry with the other designs (nothing to do here).
void sceneSetLook(bool night, Comfort comfort);
// Static lower room (wall background, floor, rug, plant, sill), drawn into g
// whose (0,0) sits at screen position (ox,oy). Used for the whole room and
// inside the cat sprite, so the moving cat never shows a box around it.
void drawSceneLower(TFT_eSPI& g, int ox, int oy);
// Repaints the lower scene inside one screen rect.
void sceneRepaintRect(int x, int y, int w, int h);
// Animated window (sky, sun/moon, clouds, rain/snow) for the last SceneInfo
// given to drawRoomClock.
void drawSceneAnim(uint32_t nowMs);
// Big clock + date/weather line (call when they change).
void drawRoomClock(const SceneInfo& s);
