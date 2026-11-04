// firmware/src/ui/room_scene.h -- the dog's garden: open sky that follows
// the hour and the live weather, hills, a wooden fence, grass and the
// doghouse; big clock + date/weather line on the sky, top left.
//
// Layout (Habitacion body 26..184):
//   clock    centred at x ROOM_CLOCK_CX, y 30..64 (FONT_CLOCK), on flat sky
//   info     "mié 4 nov · 13° Nublado" (shortened to fit), y 68..87
//   corner   x 208..304, y 30..80: own sprite, animated (sun rays, moon,
//            drifting cloud, rain / snow, butterfly / fireflies)
//   doghouse x 262..316 (static)
//   dog      sprite y ROOM_CAT_Y..+94, roams x ROOM_BOT_MIN_X..ROOM_BOT_MAX_X
//   grass    y ROOM_FLOOR_Y..ROOM_STRIP_Y
#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>
#include "../logic/room.h"

#define ROOM_FLOOR_Y   168
#define ROOM_CLOCK_CX  84
#define ROOM_BOT_MIN_X 4
#define ROOM_BOT_MAX_X 160     // sprite right edge stays left of the doghouse

enum class Sky : uint8_t { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, STORM };

struct SceneInfo {
    bool  timeKnown;
    int   hour, minute;
    int   wday, mday, mon;   // date (valid when timeKnown)
    bool  weatherOk;
    int   weatherCode;       // WMO code from Open-Meteo
    float outTemp;           // °C outside
    bool  night;             // night sky, moon, fireflies
    Comfort comfort;
};

Sky skyFromWmo(int code);

// Creates the animated-corner sprite (~9 KB). Safe to call more than once.
void sceneInit();
// Day or night (call before drawing the static scene).
void sceneSetLook(bool night, Comfort comfort);
// Static garden drawn into g whose (0,0) sits at screen position (ox,oy).
// Used for the whole screen and inside the dog sprite, so the moving dog
// never shows a box. The sky follows the last weather drawRoomClock saw.
void drawSceneLower(TFT_eSPI& g, int ox, int oy);
// Repaints the static garden inside one screen rect.
void sceneRepaintRect(int x, int y, int w, int h);
// Animated corner (never under the dog).
void drawSceneAnim(uint32_t nowMs);
// Big clock + date/weather line (call when they change). Repaints the whole
// garden first when the weather changed the sky.
void drawRoomClock(const SceneInfo& s);
