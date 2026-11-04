// firmware/src/ui/room_scene.h -- the cat's room: window (sky by hour + live
// weather), big clock, floor with rug and a plant.
//
// Layout (Habitacion body 26..184):
//   window   x 14..78,  y 32..80   own 64x48 sprite, animated
//   clock    centred at x ROOM_CLOCK_CX, y 30..64 (FONT_CLOCK)
//   info     weather ("14° · Nublado") or date, y 66..85
//   cat      sprite y ROOM_CAT_Y..+90, roams x MIN..MAX
//   floor    y ROOM_FLOOR_Y..ROOM_STRIP_Y, rug + plant
#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

#define ROOM_FLOOR_Y   170
#define ROOM_CLOCK_CX  199

enum class Sky : uint8_t { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, STORM };

struct SceneInfo {
    bool  timeKnown;
    int   hour, minute;
    int   wday, mday, mon;   // date (valid when timeKnown)
    bool  weatherOk;
    int   weatherCode;       // WMO code from Open-Meteo
    float outTemp;           // °C outside
};

Sky skyFromWmo(int code);

// Creates the window sprite (~6 KB). Safe to call more than once.
void sceneInit();
// Static lower room (wall background, floor, rug, plant), drawn into g whose
// (0,0) sits at screen position (ox,oy). Used for the whole room and inside
// the cat sprite, so the moving cat never shows a box around it.
void drawSceneLower(TFT_eSPI& g, int ox, int oy);
// Repaints the lower scene inside one screen rect (e.g. the strip the moving
// cat just left).
void sceneRepaintRect(int x, int y, int w, int h);
// Window frame + sill (static) and its animated contents.
void drawWindowFrame();
void drawWindow(uint32_t nowMs, const SceneInfo& s);
// Big clock + weather/date line (call when they change).
void drawRoomClock(const SceneInfo& s);
