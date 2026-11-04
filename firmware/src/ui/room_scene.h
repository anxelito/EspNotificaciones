// firmware/src/ui/room_scene.h -- the mascot's room: a space station with a
// big round window (stars, ringed planet), metal floor and a blinking
// control panel; big clock + date/weather line on the left.
//
// Layout (Habitacion body 26..184):
//   clock    centred at x ROOM_CLOCK_CX, y 30..64 (FONT_CLOCK)
//   info     "mié 4 nov · 13° Nublado" (shortened to fit), y 68..87
//   window   circle centre (WIN_CX, WIN_CY), radius WIN_R (static scene)
//   panel    blinking lights at x 290..312 (animated, never under the robot)
//   robot    sprite y ROOM_CAT_Y..+90, roams x ROOM_BOT_MIN_X..ROOM_BOT_MAX_X
//   floor    y ROOM_FLOOR_Y..ROOM_STRIP_Y
#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>
#include "../logic/room.h"

#define ROOM_FLOOR_Y   166
#define ROOM_CLOCK_CX  80
#define ROOM_BOT_MIN_X 8
#define ROOM_BOT_MAX_X 186     // sprite right edge stays left of the panel

enum class Sky : uint8_t { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, STORM };

struct SceneInfo {
    bool  timeKnown;
    int   hour, minute;
    int   wday, mday, mon;   // date (valid when timeKnown)
    bool  weatherOk;
    int   weatherCode;       // WMO code from Open-Meteo
    float outTemp;           // °C outside
    bool  night;             // window shows the planet's night side
    Comfort comfort;         // planet colour follows the room temperature
};

Sky skyFromWmo(int code);

// Kept for API symmetry (nothing to allocate for the station).
void sceneInit();
// Sets what the static scene looks like (call before drawing it).
void sceneSetLook(bool night, Comfort comfort);
// Static room (wall, window with space and planet, floor), drawn into g
// whose (0,0) sits at screen position (ox,oy). Used for the whole room and
// inside the robot sprite, so the moving robot never shows a box.
void drawSceneLower(TFT_eSPI& g, int ox, int oy);
// Repaints the static room inside one screen rect.
void sceneRepaintRect(int x, int y, int w, int h);
// Animated bits that never sit under the robot (control-panel lights).
void drawSceneAnim(uint32_t nowMs);
// Big clock + date/weather line (call when they change).
void drawRoomClock(const SceneInfo& s);
