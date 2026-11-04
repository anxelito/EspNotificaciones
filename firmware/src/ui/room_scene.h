// firmware/src/ui/room_scene.h -- the fish tank: the whole screen is water
// whose colour follows the room temperature (bluer when cold, greener when
// hot, darker at night), sand, rocks, seaweed and a castle; big clock +
// date/weather line on the flat water at the top.
//
// Layout (Habitacion body 26..184):
//   clock    centred at x ROOM_CLOCK_CX, y 30..64 (FONT_CLOCK), on flat water
//   info     "mié 4 nov · 13° Nublado" (shortened to fit), y 68..87
//   bubbles  x 248..296, y 28..122: own sprite, bubbles rising from the castle
//   castle   x 244..304 (static)
//   fish     sprite y ROOM_CAT_Y..+94, swims x ROOM_BOT_MIN_X..ROOM_BOT_MAX_X
//   sand     y ROOM_FLOOR_Y..ROOM_STRIP_Y
#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>
#include "../logic/room.h"

#define ROOM_FLOOR_Y   170
#define ROOM_CLOCK_CX  100
#define ROOM_BOT_MIN_X 4
#define ROOM_BOT_MAX_X 144     // sprite right edge stays left of the castle

enum class Sky : uint8_t { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, STORM };

struct SceneInfo {
    bool  timeKnown;
    int   hour, minute;
    int   wday, mday, mon;   // date (valid when timeKnown)
    bool  weatherOk;
    int   weatherCode;       // WMO code from Open-Meteo
    float outTemp;           // °C outside
    bool  night;             // darker water, slower bubbles
    Comfort comfort;         // water colour
};

Sky skyFromWmo(int code);

// Creates the bubble-column sprite (~9 KB). Safe to call more than once.
void sceneInit();
// Water colour: night and room comfort (call before drawing the scene).
void sceneSetLook(bool night, Comfort comfort);
// Static tank drawn into g whose (0,0) sits at screen position (ox,oy).
// Used for the whole screen and inside the fish sprite, so the moving fish
// never shows a box.
void drawSceneLower(TFT_eSPI& g, int ox, int oy);
// Repaints the static tank inside one screen rect.
void sceneRepaintRect(int x, int y, int w, int h);
// Bubbles rising from the castle (never under the fish).
void drawSceneAnim(uint32_t nowMs);
// Big clock + date/weather line (call when they change).
void drawRoomClock(const SceneInfo& s);
