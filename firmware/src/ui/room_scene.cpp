// firmware/src/ui/room_scene.cpp -- the fish tank
#include "room_scene.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "screen_room.h"
#include "../display.h"

// ---- palette ----------------------------------------------------------------
static constexpr uint16_t C_SAND     = rgb565(0xe6, 0xcf, 0x98);
static constexpr uint16_t C_SAND_D   = rgb565(0xc8, 0xae, 0x78);
static constexpr uint16_t C_PEBBLE   = rgb565(0xf4, 0x9a, 0x8a);
static constexpr uint16_t C_PEBBLE2  = rgb565(0x8a, 0xc4, 0xf0);
static constexpr uint16_t C_ROCK     = rgb565(0x7d, 0x85, 0x97);
static constexpr uint16_t C_ROCK_L   = rgb565(0x9a, 0xa2, 0xb4);
static constexpr uint16_t C_CASTLE   = rgb565(0xb9, 0xb2, 0xcc);
static constexpr uint16_t C_CASTLE_D = rgb565(0x94, 0x8c, 0xaa);
static constexpr uint16_t C_DOOR     = rgb565(0x1c, 0x24, 0x38);
static constexpr uint16_t C_FLAG     = rgb565(0xff, 0x5c, 0x7a);
static constexpr uint16_t C_WEED     = rgb565(0x3f, 0xbf, 0x6b);
static constexpr uint16_t C_WEED_D   = rgb565(0x2c, 0x96, 0x52);
static constexpr uint16_t C_BUBBLE   = rgb565(0xd8, 0xf2, 0xff);
static constexpr uint16_t C_GLASS    = rgb565(0xc8, 0xec, 0xff);

static const int BX = 248, BY = 28, BW = 48, BH = 94;     // bubble column sprite
static const int CLOCK_W = 200;                            // clock column x 0..200
static const int CASTLE_X = 244;
static const float PI_F = 3.14159265f;

static TFT_eSprite bub(&tft);
static bool haveBub = false;

static bool    lookNight = false;
static Comfort lookComfort = Comfort::UNKNOWN;

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
    if (haveBub) return;
    bub.setColorDepth(16);
    haveBub = bub.createSprite(BW, BH) != nullptr;
    if (!haveBub) Serial.println("[scene] bubble sprite alloc failed");
}

void sceneSetLook(bool night, Comfort comfort) {
    lookNight = night;
    lookComfort = comfort;
}

// Water: flat top band (the clock sits on it), then three deeper bands.
static uint16_t waterTop() {
    uint16_t c;
    switch (lookComfort) {
        case Comfort::COLD: c = rgb565(0x2a, 0x72, 0xc8); break;
        case Comfort::HOT:  c = rgb565(0x26, 0x8a, 0x7a); break;
        default:            c = rgb565(0x22, 0x80, 0xb8); break;
    }
    return lookNight ? mix565(c, rgb565(0x04, 0x08, 0x18), 90) : c;
}

static uint16_t waterBand(int i) {                          // 0 = top .. 3 = deepest
    return mix565(waterTop(), rgb565(0x04, 0x10, 0x30), (uint8_t)(255 - i * 34));
}

static uint16_t dim(uint16_t c) { return lookNight ? mix565(c, rgb565(0x04, 0x08, 0x18), 110) : c; }

// ---- static tank -------------------------------------------------------------------

static void weed(TFT_eSPI& g, int x, int base, int h, int phase, uint16_t c, int ox, int oy) {
    int px = x, py = base;
    for (int k = 1; k <= h / 10; k++) {
        int nx = x + (int)lroundf(5.0f * sinf(k * 0.9f + phase));
        int ny = base - k * 10;
        for (int d = -2; d <= 2; d++) g.drawLine(px + d - ox, py - oy, nx + d - ox, ny - oy, c);
        px = nx; py = ny;
    }
    g.fillCircle(px - ox, py - oy, 3, c);
}

static void castle(TFT_eSPI& g, int ox, int oy) {
    const int x = CASTLE_X - ox, base = ROOM_FLOOR_Y + 2 - oy;
    const uint16_t c = dim(C_CASTLE), cd = dim(C_CASTLE_D);
    g.fillRect(x + 8, base - 40, 44, 42, c);                 // keep
    g.fillRect(x, base - 54, 16, 56, c);                     // left tower
    g.fillRect(x + 44, base - 54, 16, 56, c);                // right tower
    for (int i = 0; i < 3; i++) {                            // battlements
        g.fillRect(x + i * 6, base - 60, 4, 7, c);
        g.fillRect(x + 44 + i * 6, base - 60, 4, 7, c);
        g.fillRect(x + 14 + i * 12, base - 46, 6, 7, c);
    }
    g.drawFastVLine(x + 16, base - 40, 42, cd);
    g.drawFastVLine(x + 43, base - 40, 42, cd);
    for (int y = base - 30; y < base; y += 10) g.drawFastHLine(x + 17, y, 26, cd);
    g.fillRoundRect(x + 23, base - 22, 14, 24, 6, C_DOOR);
    g.fillRoundRect(x + 5, base - 44, 6, 9, 3, C_DOOR);
    g.fillRoundRect(x + 49, base - 44, 6, 9, 3, C_DOOR);
    g.drawFastVLine(x + 52, base - 72, 13, cd);
    g.fillTriangle(x + 53, base - 72, x + 53, base - 65, x + 61, base - 69, dim(C_FLAG));
}

void drawSceneLower(TFT_eSPI& g, int ox, int oy) {
    // Water bands.
    const int bands[] = { HEADER_H, 92, 118, 144, ROOM_FLOOR_Y };
    for (int i = 0; i < 4; i++)
        g.fillRect(-ox, bands[i] - oy, SCREEN_W, bands[i + 1] - bands[i], waterBand(i));
    // Light shafts (day) below the clock: slanted lighter stripes.
    if (!lookNight) {
        for (int k = 0; k < 3; k++) {
            int x0 = 40 + k * 90;
            for (int y = 92; y < ROOM_FLOOR_Y; y += 2) {
                int band = y < 118 ? 1 : (y < 144 ? 2 : 3);
                int x = x0 + (y - 92) / 3;
                g.drawFastHLine(x - ox, y - oy, 14, mix565(0xFFFF, waterBand(band), 26));
            }
        }
    }
    // Glass reflection on the left edge.
    g.drawFastVLine(3 - ox, 96 - oy, 50, mix565(C_GLASS, waterBand(1), 60));
    g.drawFastVLine(5 - ox, 104 - oy, 22, mix565(C_GLASS, waterBand(1), 40));

    // Seaweed + rocks.
    weed(g, 16, ROOM_FLOOR_Y + 4, 56, 0, dim(C_WEED_D), ox, oy);
    weed(g, 28, ROOM_FLOOR_Y + 4, 40, 2, dim(C_WEED), ox, oy);
    weed(g, 214, ROOM_FLOOR_Y + 4, 62, 1, dim(C_WEED), ox, oy);
    weed(g, 226, ROOM_FLOOR_Y + 4, 44, 3, dim(C_WEED_D), ox, oy);
    g.fillEllipse(66 - ox, ROOM_FLOOR_Y + 2 - oy, 16, 9, dim(C_ROCK));
    g.fillEllipse(62 - ox, ROOM_FLOOR_Y - 2 - oy, 8, 3, dim(C_ROCK_L));
    g.fillEllipse(86 - ox, ROOM_FLOOR_Y + 4 - oy, 10, 6, dim(C_ROCK_L));

    // Sand with a wavy top and pebbles.
    for (int x = 0; x < SCREEN_W; x++) {
        int top = ROOM_FLOOR_Y + (int)lroundf(2.0f * sinf(x / 22.0f));
        g.drawFastVLine(x - ox, top - oy, ROOM_STRIP_Y - top, dim(C_SAND));
    }
    for (int i = 0; i < 20; i++) {
        int px = (i * 47 + 11) % SCREEN_W, py = ROOM_FLOOR_Y + 5 + (i * 7) % 9;
        g.fillCircle(px - ox, py - oy, 1, dim(i % 3 == 0 ? C_PEBBLE : (i % 3 == 1 ? C_PEBBLE2 : C_SAND_D)));
    }
    castle(g, ox, oy);
}

void sceneRepaintRect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.setViewport(x, y, w, h, false);   // absolute coords, clipped to the rect
    drawSceneLower(tft, 0, 0);
    tft.resetViewport();
}

// ---- bubbles -----------------------------------------------------------------------

void drawSceneAnim(uint32_t now) {
    if (!haveBub) return;
    TFT_eSPI& g = bub;
    drawSceneLower(g, BX, BY);
    const uint32_t speed = lookNight ? 2 : 1;               // slower at night
    for (int i = 0; i < 7; i++) {
        uint32_t period = (2600 + i * 230) * speed;
        float ph = (float)((now + i * 977) % period) / (float)period;
        int x = (i % 2 ? 266 : 286) + (int)lroundf(3.0f * sinf(ph * 12.0f + i));   // from the towers
        int y = ROOM_FLOOR_Y - 60 - (int)(ph * (BH - 8));
        if (y < BY + 2) continue;
        int r = 1 + i % 3;
        uint16_t c = mix565(C_BUBBLE, waterBand(y < 92 ? 0 : 1), 170);
        g.drawCircle(x - BX, y - BY, r, c);
        g.drawPixel(x - BX - (r > 1 ? 1 : 0), y - BY - (r > 1 ? 1 : 0), C_BUBBLE);
    }
    bub.pushSprite(BX, BY);
}

// ---- clock + info line ------------------------------------------------------------

void drawRoomClock(const SceneInfo& s) {
    sceneRepaintRect(0, HEADER_H, CLOCK_W, ROOM_CAT_Y - HEADER_H);
    const uint16_t bg = waterTop();
    const int maxW = CLOCK_W - 2 * MARGIN;

    char buf[48];
    if (s.timeKnown) snprintf(buf, sizeof(buf), "%02d:%02d", s.hour, s.minute);
    else             strlcpy(buf, "--:--", sizeof(buf));
    drawTextCentre(buf, ROOM_CLOCK_CX, 30, UiFont::CLOCK, C_TEXT, bg);

    static const char* const DAYS[] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
    static const char* const MONTHS[] = { "ene", "feb", "mar", "abr", "may", "jun",
                                          "jul", "ago", "sep", "oct", "nov", "dic" };
    const bool haveDate = s.timeKnown && s.wday >= 0 && s.wday < 7 && s.mon >= 0 && s.mon < 12;
    const char* day = haveDate ? DAYS[s.wday] : "";
    const char* mon = haveDate ? MONTHS[s.mon] : "";
    const int t = (int)lroundf(s.outTemp);
    const char* word = s.weatherOk ? skyWord(skyFromWmo(s.weatherCode)) : "";

    char cand[5][48] = {};
    int n = 0;
    if (haveDate && s.weatherOk) {
        snprintf(cand[n++], 48, "%s %d %s · %d° %s", day, s.mday, mon, t, word);
        snprintf(cand[n++], 48, "%s %d · %d° %s", day, s.mday, t, word);
        snprintf(cand[n++], 48, "%s %d %s · %d°", day, s.mday, mon, t);
        snprintf(cand[n++], 48, "%s %d · %d°", day, s.mday, t);
    } else if (haveDate) {
        snprintf(cand[n++], 48, "%s %d %s", day, s.mday, mon);
    } else if (s.weatherOk) {
        snprintf(cand[n++], 48, "%d° %s", t, word);
        snprintf(cand[n++], 48, "%d°", t);
    }
    const uint16_t infoCol = mix565(C_TEXT, bg, 190);
    for (int i = 0; i < n; i++) {
        if (textWidthIn(cand[i], UiFont::SMALL) <= maxW || i == n - 1) {
            char fit[48];
            fitText(cand[i], fit, sizeof(fit), maxW, UiFont::SMALL);
            drawTextCentre(fit, ROOM_CLOCK_CX, 68, UiFont::SMALL, infoCol, bg);
            break;
        }
    }
}
