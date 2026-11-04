// firmware/src/ui/room_scene.cpp
#include "room_scene.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "screen_room.h"
#include "../display.h"

// ---- palette ----------------------------------------------------------------
static constexpr uint16_t C_FLOOR  = rgb565(0x16, 0x15, 0x1d);
static constexpr uint16_t C_BOARD  = rgb565(0x24, 0x2a, 0x3a);
static constexpr uint16_t C_RUG    = rgb565(0x22, 0x2a, 0x40);
static constexpr uint16_t C_RUG2   = rgb565(0x33, 0x3e, 0x5e);
static constexpr uint16_t C_POT    = rgb565(0xb8, 0x62, 0x3c);
static constexpr uint16_t C_POT_D  = rgb565(0x8a, 0x45, 0x2a);
static constexpr uint16_t C_LEAF   = rgb565(0x3f, 0xa6, 0x6b);
static constexpr uint16_t C_LEAF_D = rgb565(0x2a, 0x7a, 0x4e);
static constexpr uint16_t C_FRAME  = rgb565(0x3a, 0x42, 0x56);
static constexpr uint16_t C_SILL   = rgb565(0x2c, 0x33, 0x44);
static constexpr uint16_t C_SUN    = rgb565(0xff, 0xd7, 0x5a);
static constexpr uint16_t C_MOON   = rgb565(0xf3, 0xf0, 0xd0);
static constexpr uint16_t C_CLOUD  = rgb565(0xe8, 0xee, 0xf6);
static constexpr uint16_t C_CLOUD_D= rgb565(0x8e, 0x98, 0xab);
static constexpr uint16_t C_RAIN   = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_BOLT   = rgb565(0xff, 0xe8, 0x6a);

// ---- window sprite -----------------------------------------------------------
static const int WIN_X = 12, WIN_Y = 30, WIN_W = 68, WIN_H = 52;
static const int IN_X = 3, IN_Y = 3, IN_W = WIN_W - 6, IN_H = WIN_H - 6;   // sky area
static TFT_eSprite win(&tft);
static bool haveWin = false;

static const float PI_F = 3.14159265f;

Sky skyFromWmo(int c) {
    if (c == 0) return Sky::CLEAR;
    if (c == 1 || c == 2) return Sky::PARTLY;
    if (c == 3) return Sky::CLOUDY;
    if (c == 45 || c == 48) return Sky::FOG;
    if (c >= 51 && c <= 57) return Sky::DRIZZLE;
    if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return Sky::RAIN;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return Sky::SNOW;
    if (c >= 95) return Sky::STORM;
    return Sky::CLOUDY;
}

static const char* skyWord(Sky s) {
    switch (s) {
        case Sky::CLEAR:   return "Despejado";
        case Sky::PARTLY:  return "Poco nuboso";
        case Sky::CLOUDY:  return "Nublado";
        case Sky::FOG:     return "Niebla";
        case Sky::DRIZZLE: return "Llovizna";
        case Sky::RAIN:    return "Lluvia";
        case Sky::SNOW:    return "Nieve";
        default:           return "Tormenta";
    }
}

void sceneInit() {
    if (haveWin) return;
    win.setColorDepth(16);
    haveWin = win.createSprite(WIN_W, WIN_H) != nullptr;
    if (!haveWin) Serial.println("[scene] window sprite alloc failed");
}

// ---- lower room ----------------------------------------------------------------

static void plant(TFT_eSPI& g, int ox, int oy) {
    const int px = 296 - ox, py = 170 - oy;           // pot bottom centre
    // Leaves behind the pot (big drooping ellipses + a few upright ones).
    g.fillEllipse(px - 9, py - 34, 5, 12, C_LEAF_D);
    g.fillEllipse(px + 9, py - 33, 5, 12, C_LEAF_D);
    g.fillEllipse(px, py - 44, 5, 14, C_LEAF);
    g.fillEllipse(px - 14, py - 26, 7, 4, C_LEAF);
    g.fillEllipse(px + 14, py - 25, 7, 4, C_LEAF);
    g.drawLine(px, py - 22, px, py - 52, C_LEAF_D);
    // Pot: tapered body + rim.
    g.fillTriangle(px - 11, py - 20, px + 11, py - 20, px - 8, py, C_POT);
    g.fillTriangle(px + 11, py - 20, px + 8, py, px - 8, py, C_POT);
    g.fillRect(px - 13, py - 24, 26, 5, C_POT_D);
}

void drawSceneLower(TFT_eSPI& g, int ox, int oy) {
    // Wall and floor, relative to g's origin (covers whatever g can show).
    g.fillRect(-ox, HEADER_H - oy, SCREEN_W, ROOM_FLOOR_Y - HEADER_H, C_BG);
    g.fillRect(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, ROOM_STRIP_Y - ROOM_FLOOR_Y, C_FLOOR);
    g.drawFastHLine(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, C_BOARD);
    g.drawFastHLine(-ox, ROOM_FLOOR_Y + 1 - oy, SCREEN_W, C_BOARD);
    // Rug in the middle of the floor.
    g.fillEllipse(150 - ox, 177 - oy, 78, 6, C_RUG2);
    g.fillEllipse(150 - ox, 177 - oy, 72, 4, C_RUG);
    plant(g, ox, oy);
}

void sceneRepaintRect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.setViewport(x, y, w, h, false);   // absolute coords, clipped to the rect
    drawSceneLower(tft, 0, 0);
    tft.resetViewport();
}

// ---- window ----------------------------------------------------------------------

static uint16_t lerp565(uint16_t a, uint16_t b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return mix565(b, a, (uint8_t)(t * 255.0f));
}

// Sky gradient colours for the hour (fractional).
static void skyColours(const SceneInfo& s, uint16_t& top, uint16_t& bot, bool& night) {
    float h = s.timeKnown ? s.hour + s.minute / 60.0f : 12.0f;
    night = h < 6.5f || h >= 20.5f;
    if (night)           { top = rgb565(0x0a, 0x12, 0x2c); bot = rgb565(0x1b, 0x27, 0x52); }
    else if (h < 8.5f)   { top = rgb565(0x3b, 0x4a, 0x8a); bot = rgb565(0xff, 0x9e, 0x6b); }
    else if (h < 18.5f)  { top = rgb565(0x3f, 0x95, 0xe6); bot = rgb565(0xa6, 0xd8, 0xff); }
    else                 { top = rgb565(0x4b, 0x3b, 0x7a); bot = rgb565(0xff, 0x7a, 0x59); }
}

static void cloud(TFT_eSprite& g, int x, int y, uint16_t col) {
    g.fillCircle(x, y, 5, col);
    g.fillCircle(x + 7, y - 3, 6, col);
    g.fillCircle(x + 14, y, 5, col);
    g.fillRect(x, y, 15, 5, col);
}

void drawWindow(uint32_t now, const SceneInfo& s) {
    if (!haveWin) return;
    TFT_eSprite& g = win;
    g.fillRoundRect(0, 0, WIN_W, WIN_H, 6, C_FRAME);

    uint16_t top, bot;
    bool night;
    skyColours(s, top, bot, night);
    const Sky sky = s.weatherOk ? skyFromWmo(s.weatherCode) : Sky::CLEAR;
    const bool stormFlash = sky == Sky::STORM && (now % 4200) < 110;
    const bool overcast = sky == Sky::CLOUDY || sky == Sky::RAIN || sky == Sky::DRIZZLE ||
                          sky == Sky::SNOW || sky == Sky::STORM || sky == Sky::FOG;
    if (overcast) {                                    // greyer sky
        top = mix565(top, C_CLOUD_D, 150);
        bot = mix565(bot, C_CLOUD_D, 150);
    }
    for (int y = 0; y < IN_H; y++) {
        uint16_t c = stormFlash ? rgb565(0xdd, 0xe4, 0xff) : lerp565(top, bot, (float)y / IN_H);
        g.drawFastHLine(IN_X, IN_Y + y, IN_W, c);
    }

    // Stars (night, not overcast), twinkling.
    if (night && !overcast) {
        static const uint8_t ST[][2] = { {6, 6}, {15, 14}, {24, 4}, {9, 25}, {30, 18},
                                         {52, 30}, {57, 8}, {20, 34}, {40, 38}, {48, 22} };
        for (int i = 0; i < 10; i++) {
            bool dim = ((now / 450) + i * 3) % 7 == 0;
            g.drawPixel(IN_X + ST[i][0], IN_Y + ST[i][1], dim ? top : C_MOON);
        }
    }

    // Sun or moon (unless fully overcast).
    if (!overcast) {
        int cx = IN_X + 45, cy = IN_Y + 12;
        if (night) {
            g.fillCircle(cx, cy, 7, C_MOON);
            g.fillCircle(cx + 4, cy - 3, 6, lerp565(top, bot, (float)(cy - 3 - IN_Y) / IN_H));
        } else {
            float a0 = (float)(now % 12000) / 12000.0f * 2.0f * PI_F;
            for (int i = 0; i < 8; i++) {
                float a = a0 + i * PI_F / 4.0f;
                g.drawLine(cx + (int)(10 * cosf(a)), cy + (int)(10 * sinf(a)),
                           cx + (int)(13 * cosf(a)), cy + (int)(13 * sinf(a)), C_SUN);
            }
            g.fillCircle(cx, cy, 7, C_SUN);
        }
    }

    // Clouds drifting left to right.
    uint16_t cc = (sky == Sky::RAIN || sky == Sky::STORM) ? C_CLOUD_D : C_CLOUD;
    if (night) cc = mix565(cc, top, 140);
    int span = IN_W + 30;
    if (sky != Sky::CLEAR && sky != Sky::FOG) {
        int x1 = (int)((now / 140) % span) - 20;
        cloud(g, IN_X + x1, IN_Y + 14, cc);
        if (sky != Sky::PARTLY) {
            int x2 = (int)((now / 200 + span / 2) % span) - 20;
            cloud(g, IN_X + x2, IN_Y + 24, mix565(cc, C_CLOUD_D, 160));
        }
    }

    // Precipitation.
    if (sky == Sky::RAIN || sky == Sky::DRIZZLE || sky == Sky::STORM) {
        int n = sky == Sky::DRIZZLE ? 7 : 14;
        for (int i = 0; i < n; i++) {
            int x = IN_X + (i * 13 + 5) % IN_W;
            int y = IN_Y + 18 + (int)((i * 17 + now / (18 + (i % 3) * 6)) % (IN_H - 14));
            g.drawFastVLine(x, y, sky == Sky::DRIZZLE ? 2 : 4, C_RAIN);
        }
    } else if (sky == Sky::SNOW) {
        for (int i = 0; i < 12; i++) {
            int y = IN_Y + (int)((i * 11 + now / (70 + (i % 4) * 15)) % IN_H);
            int x = IN_X + (i * 13 + 4) % IN_W + (int)(2.0f * sinf((now / 400.0f) + i));
            g.fillCircle(x, y, 1, C_CLOUD);
        }
    } else if (sky == Sky::FOG) {
        for (int i = 0; i < 3; i++) {
            int y = IN_Y + 10 + i * 12;
            int dx = (int)((now / (90 + i * 40)) % 20) - 10;
            g.fillRoundRect(IN_X - 10 + dx, y, IN_W + 20, 5, 2, mix565(C_CLOUD, top, 120));
        }
    }
    if (sky == Sky::STORM && (now % 4200) < 300) {     // lightning bolt
        int bx = IN_X + 30, by = IN_Y + 22;
        g.fillTriangle(bx, by, bx + 6, by, bx + 1, by + 9, C_BOLT);
        g.fillTriangle(bx + 1, by + 7, bx + 7, by + 7, bx - 1, by + 18, C_BOLT);
    }

    // Window bars over everything.
    g.fillRect(IN_X + IN_W / 2 - 1, IN_Y, 2, IN_H, C_FRAME);
    g.fillRect(IN_X, IN_Y + IN_H / 2 - 1, IN_W, 2, C_FRAME);
    g.pushSprite(WIN_X, WIN_Y);
}

void drawWindowFrame() {
    // Sill under the window (static, outside the sprite).
    tft.fillRoundRect(WIN_X - 4, WIN_Y + WIN_H - 1, WIN_W + 8, 5, 2, C_SILL);
}

// ---- clock + info line ---------------------------------------------------------

void drawRoomClock(const SceneInfo& s) {
    const int x0 = WIN_X + WIN_W + 6;
    tft.fillRect(x0, HEADER_H + 1, SCREEN_W - x0, ROOM_CAT_Y - HEADER_H - 1, C_BG);

    char buf[48];
    if (s.timeKnown) snprintf(buf, sizeof(buf), "%02d:%02d", s.hour, s.minute);
    else             strlcpy(buf, "--:--", sizeof(buf));
    drawTextCentre(buf, ROOM_CLOCK_CX, 30, UiFont::CLOCK, C_TEXT, C_BG);

    static const char* const DAYS[] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
    static const char* const MONTHS[] = { "ene", "feb", "mar", "abr", "may", "jun",
                                          "jul", "ago", "sep", "oct", "nov", "dic" };
    char date[20] = "";
    if (s.timeKnown && s.wday >= 0 && s.wday < 7 && s.mon >= 0 && s.mon < 12)
        snprintf(date, sizeof(date), "%s %d %s", DAYS[s.wday], s.mday, MONTHS[s.mon]);
    if (s.weatherOk) {
        snprintf(buf, sizeof(buf), "%s%s%d° %s", date, date[0] ? " · " : "",
                 (int)lroundf(s.outTemp), skyWord(skyFromWmo(s.weatherCode)));
    } else {
        strlcpy(buf, date, sizeof(buf));
    }
    if (buf[0]) {
        char fit[48];
        fitText(buf, fit, sizeof(fit), SCREEN_W - x0 - MARGIN, UiFont::SMALL);
        drawTextCentre(fit, ROOM_CLOCK_CX, 66, UiFont::SMALL, C_MUTED, C_BG);
    }
}
