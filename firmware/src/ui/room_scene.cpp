// firmware/src/ui/room_scene.cpp -- the dog's garden
#include "room_scene.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "screen_room.h"
#include "../display.h"

// ---- palette ----------------------------------------------------------------
static constexpr uint16_t C_SUN      = rgb565(0xff, 0xd7, 0x5a);
static constexpr uint16_t C_SUN_RAY  = rgb565(0xff, 0xe8, 0x9a);
static constexpr uint16_t C_MOON     = rgb565(0xf3, 0xf0, 0xd0);
static constexpr uint16_t C_STAR     = rgb565(0xf3, 0xf0, 0xd0);
static constexpr uint16_t C_CLOUD    = rgb565(0xf2, 0xf5, 0xfa);
static constexpr uint16_t C_CLOUD_G  = rgb565(0xb4, 0xbe, 0xcc);
static constexpr uint16_t C_CLOUD_N  = rgb565(0x4a, 0x55, 0x6e);
static constexpr uint16_t C_RAIN     = rgb565(0xd0, 0xea, 0xff);
static constexpr uint16_t C_SNOW     = rgb565(0xff, 0xff, 0xff);
static constexpr uint16_t C_HILL     = rgb565(0x6c, 0xc4, 0x78);
static constexpr uint16_t C_HILL2    = rgb565(0x5a, 0xb4, 0x68);
static constexpr uint16_t C_GRASS    = rgb565(0x4a, 0xa4, 0x58);
static constexpr uint16_t C_GRASS_D  = rgb565(0x3a, 0x8a, 0x48);
static constexpr uint16_t C_FENCE    = rgb565(0xee, 0xdc, 0xbc);
static constexpr uint16_t C_FENCE_D  = rgb565(0xc8, 0xb2, 0x8c);
static constexpr uint16_t C_HOUSE    = rgb565(0xd0, 0x5a, 0x44);
static constexpr uint16_t C_HOUSE_D  = rgb565(0xa8, 0x44, 0x34);
static constexpr uint16_t C_ROOF     = rgb565(0x6e, 0x3a, 0x2e);
static constexpr uint16_t C_DOOR     = rgb565(0x2a, 0x17, 0x14);
static constexpr uint16_t C_BONE     = rgb565(0xf4, 0xee, 0xe0);
static constexpr uint16_t C_FLOWER   = rgb565(0xff, 0x8a, 0xb0);
static constexpr uint16_t C_FLOWER2  = rgb565(0xff, 0xe2, 0x6a);
static constexpr uint16_t C_WING     = rgb565(0xff, 0xa8, 0x3a);
static constexpr uint16_t C_FIREFLY  = rgb565(0xff, 0xf2, 0x7a);
static constexpr uint16_t C_PUDDLE   = rgb565(0x8c, 0xb8, 0xdc);

static const int AX = 208, AY = 30, AW = 96, AH = 50;     // animated corner
static const int CLOCK_W = 170;                            // clock column x 0..170
static const int HOUSE_X = 262;
static const float PI_F = 3.14159265f;

static TFT_eSprite anim(&tft);
static bool haveAnim = false;

static bool lookNight = false;
enum class Wx : uint8_t { CLEAR, PARTLY, CLOUDY, RAIN, SNOW };
static Wx   wx = Wx::CLEAR;

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

static Wx wxOf(Sky s) {
    switch (s) {
        case Sky::CLEAR:   return Wx::CLEAR;
        case Sky::PARTLY:  return Wx::PARTLY;
        case Sky::SNOW:    return Wx::SNOW;
        case Sky::DRIZZLE: case Sky::RAIN: case Sky::STORM: return Wx::RAIN;
        default:           return Wx::CLOUDY;
    }
}

void sceneInit() {
    if (haveAnim) return;
    anim.setColorDepth(16);
    haveAnim = anim.createSprite(AW, AH) != nullptr;
    if (!haveAnim) Serial.println("[scene] corner sprite alloc failed");
}

void sceneSetLook(bool night, Comfort) { lookNight = night; }

// Flat sky colour (the clock is drawn on it, so it has no gradient).
static uint16_t skyColour() {
    const bool grey = wx == Wx::CLOUDY || wx == Wx::RAIN || wx == Wx::SNOW;
    if (lookNight) return grey ? rgb565(0x1c, 0x22, 0x36) : rgb565(0x0e, 0x18, 0x3a);
    switch (wx) {
        case Wx::RAIN:   return rgb565(0x6e, 0x80, 0x98);
        case Wx::SNOW:   return rgb565(0x86, 0x98, 0xb2);
        case Wx::CLOUDY: return rgb565(0x7c, 0x9c, 0xc0);
        default:         return rgb565(0x4e, 0xa2, 0xe6);
    }
}

static uint16_t night(uint16_t c) { return lookNight ? mix565(c, rgb565(0x0a, 0x10, 0x28), 90) : c; }

static void cloud(TFT_eSPI& g, int x, int y, int s, uint16_t col) {
    g.fillCircle(x, y, 5 * s / 2, col);
    g.fillCircle(x + 7 * s / 2, y - 3 * s / 2, 3 * s, col);
    g.fillCircle(x + 7 * s, y, 5 * s / 2, col);
    g.fillRect(x, y, 7 * s, 5 * s / 2 + 1, col);
}

static uint16_t cloudColour() {
    if (lookNight) return C_CLOUD_N;
    return (wx == Wx::RAIN || wx == Wx::SNOW || wx == Wx::CLOUDY) ? C_CLOUD_G : C_CLOUD;
}

// ---- static garden --------------------------------------------------------------

static void doghouse(TFT_eSPI& g, int ox, int oy) {
    const int x = HOUSE_X - ox, base = ROOM_FLOOR_Y - oy;
    g.fillRect(x + 4, base - 48, 48, 50, night(C_HOUSE));
    for (int y = base - 40; y < base; y += 8) g.drawFastHLine(x + 4, y, 48, night(C_HOUSE_D));
    g.fillTriangle(x - 3, base - 46, x + 59, base - 46, x + 28, base - 74, night(C_ROOF));
    g.fillRect(x - 3, base - 48, 62, 4, night(C_ROOF));
    g.fillRoundRect(x + 16, base - 32, 24, 34, 11, C_DOOR);
    // Name plate + bowl.
    g.fillRoundRect(x + 18, base - 44, 20, 7, 2, night(C_FENCE));
    g.fillRect(x + 22, base - 41, 12, 1, night(C_HOUSE_D));
    g.fillEllipse(x + 2, base + 4, 9, 3, night(rgb565(0x5a, 0x8c, 0xd8)));
    g.fillEllipse(x + 2, base + 3, 7, 1, night(C_BONE));
    if (wx == Wx::SNOW) {                                   // snow on the roof
        g.fillTriangle(x + 2, base - 50, x + 54, base - 50, x + 28, base - 72, C_SNOW);
        g.fillRect(x - 3, base - 50, 62, 3, C_SNOW);
    }
}

void drawSceneLower(TFT_eSPI& g, int ox, int oy) {
    const uint16_t sky = skyColour();
    g.fillRect(-ox, HEADER_H - oy, SCREEN_W, ROOM_FLOOR_Y - HEADER_H, sky);

    // Sun / moon + stars (rays and twinkle are animated in the corner).
    if (lookNight) {
        if (wx == Wx::CLEAR || wx == Wx::PARTLY) {
            for (int i = 0; i < 22; i++) {
                int sx = CLOCK_W + (i * 67) % (SCREEN_W - CLOCK_W), sy = 30 + (i * 29) % 80;
                g.drawPixel(sx - ox, sy - oy, C_STAR);
            }
        }
        g.fillCircle(276 - ox, 50 - oy, 11, C_MOON);
        g.fillCircle(282 - ox, 45 - oy, 10, sky);
    } else if (wx == Wx::CLEAR || wx == Wx::PARTLY) {
        g.fillCircle(276 - ox, 50 - oy, 11, C_SUN);
    }

    // Static clouds low in the sky (never behind the clock).
    const uint16_t cc = cloudColour();
    if (wx != Wx::CLEAR) {
        cloud(g, 186 - ox, 100 - oy, 3, cc);
        if (wx != Wx::PARTLY) cloud(g, 40 - ox, 104 - oy, 2, cc);
    }
    if (wx == Wx::RAIN) {                                   // fixed streaks low in the sky
        for (int i = 0; i < 26; i++) {
            int rx = (i * 47 + 13) % SCREEN_W, ry = 96 + (i * 31) % 40;
            g.drawFastVLine(rx - ox, ry - oy, 4, mix565(C_RAIN, sky, 150));
        }
    }

    // Hills and fence.
    const uint16_t hill = wx == Wx::SNOW ? rgb565(0xe6, 0xee, 0xf6) : C_HILL;
    const uint16_t hill2 = wx == Wx::SNOW ? rgb565(0xd4, 0xde, 0xea) : C_HILL2;
    g.fillEllipse(70 - ox, 150 - oy, 120, 34, night(hill));
    g.fillEllipse(250 - ox, 156 - oy, 130, 36, night(hill2));
    for (int x = 4; x < SCREEN_W; x += 26) {
        g.fillRect(x - ox, 128 - oy, 8, ROOM_FLOOR_Y - 128, night(C_FENCE));
        g.fillTriangle(x - ox, 128 - oy, x + 7 - ox, 128 - oy, x + 3 - ox, 123 - oy, night(C_FENCE));
        g.drawFastVLine(x + 7 - ox, 128 - oy, ROOM_FLOOR_Y - 128, night(C_FENCE_D));
    }
    g.fillRect(-ox, 136 - oy, SCREEN_W, 4, night(C_FENCE_D));
    g.fillRect(-ox, 152 - oy, SCREEN_W, 4, night(C_FENCE_D));

    // Grass (or snow) with tufts and a few flowers.
    const bool snow = wx == Wx::SNOW;
    const uint16_t gr = snow ? rgb565(0xf0, 0xf4, 0xfa) : C_GRASS;
    const uint16_t grd = snow ? rgb565(0xc8, 0xd4, 0xe4) : C_GRASS_D;
    g.fillRect(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, ROOM_STRIP_Y - ROOM_FLOOR_Y, night(gr));
    g.drawFastHLine(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, night(grd));
    for (int i = 0; i < 18; i++) {
        int tx = (i * 37 + 7) % SCREEN_W, ty = ROOM_FLOOR_Y + 4 + (i * 5) % 10;
        g.drawLine(tx - ox, ty - oy, tx - 2 - ox, ty - 4 - oy, night(grd));
        g.drawLine(tx - ox, ty - oy, tx + 2 - ox, ty - 4 - oy, night(grd));
    }
    if (!snow) {
        static const int fx[] = { 22, 120, 214, 248 };
        for (int i = 0; i < 4; i++) {
            int x = fx[i] - ox, y = ROOM_FLOOR_Y + 6 + (i % 2) * 5 - oy;
            uint16_t pc = night(i % 2 ? C_FLOWER2 : C_FLOWER);
            g.fillCircle(x - 2, y, 2, pc); g.fillCircle(x + 2, y, 2, pc);
            g.fillCircle(x, y - 2, 2, pc); g.fillCircle(x, y + 2, 2, pc);
            g.fillCircle(x, y, 1, night(C_FLOWER2 == pc ? C_FLOWER : C_FLOWER2));
        }
    }
    if (wx == Wx::RAIN) {                                   // puddles
        g.fillEllipse(60 - ox, ROOM_FLOOR_Y + 9 - oy, 18, 3, night(C_PUDDLE));
        g.fillEllipse(196 - ox, ROOM_FLOOR_Y + 12 - oy, 14, 2, night(C_PUDDLE));
    }
    doghouse(g, ox, oy);
}

void sceneRepaintRect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.setViewport(x, y, w, h, false);   // absolute coords, clipped to the rect
    drawSceneLower(tft, 0, 0);
    tft.resetViewport();
}

// ---- animated corner -------------------------------------------------------------

void drawSceneAnim(uint32_t now) {
    if (!haveAnim) return;                 // static sun/moon stay; no flicker
    TFT_eSPI& g = anim;
    const int ox = AX, oy = AY;
    drawSceneLower(g, ox, oy);
    const uint16_t sky = skyColour();

    if (!lookNight && (wx == Wx::CLEAR || wx == Wx::PARTLY)) {     // turning rays
        float a0 = (float)(now % 12000) / 12000.0f * 2.0f * PI_F;
        for (int i = 0; i < 8; i++) {
            float a = a0 + i * PI_F / 4.0f;
            int x0 = 276 + (int)lroundf(cosf(a) * 14), y0 = 50 + (int)lroundf(sinf(a) * 14);
            int x1 = 276 + (int)lroundf(cosf(a) * 19), y1 = 50 + (int)lroundf(sinf(a) * 19);
            g.drawLine(x0 - ox, y0 - oy, x1 - ox, y1 - oy, C_SUN_RAY);
            g.drawLine(x0 + 1 - ox, y0 - oy, x1 + 1 - ox, y1 - oy, C_SUN_RAY);
        }
    }
    if (lookNight && (wx == Wx::CLEAR || wx == Wx::PARTLY)) {      // twinkling stars
        for (int i = 0; i < 5; i++) {
            int sx = AX + 8 + (i * 23) % (AW - 30), sy = AY + 6 + (i * 17) % (AH - 12);
            bool on = ((now / 400) + i * 3) % 5 != 0;
            if (on) { g.drawPixel(sx - ox, sy - oy, C_STAR); g.drawPixel(sx + 1 - ox, sy - oy, C_STAR);
                      g.drawPixel(sx - ox, sy - 1 - oy, C_STAR); g.drawPixel(sx - ox, sy + 1 - oy, C_STAR); }
        }
    }
    if (wx != Wx::CLEAR) {                                         // drifting cloud
        const int range = AW - 40;                                 // drifts to and fro
        int pos = (int)((now / 140) % (2 * range));
        if (pos > range) pos = 2 * range - pos;
        cloud(g, AX + 8 + pos - ox, AY + 34 - oy, 3, cloudColour());
    }
    if (wx == Wx::RAIN) {
        for (int i = 0; i < 14; i++) {
            int rx = AX + (i * 37) % AW, ry = AY + (int)((i * 23 + now / (6 + i % 3)) % AH);
            g.drawFastVLine(rx - ox, ry - oy, 5, C_RAIN);
        }
    } else if (wx == Wx::SNOW) {
        for (int i = 0; i < 12; i++) {
            int ry = AY + (int)((i * 19 + now / (40 + (i % 4) * 9)) % AH);
            int rx = AX + (i * 41) % AW + (int)lroundf(sinf(now / 500.0f + i) * 3);
            g.fillCircle(rx - ox, ry - oy, 1, C_SNOW);
        }
    } else if (!lookNight) {                                       // butterfly
        float u = (float)(now % 9000) / 9000.0f;
        int bx = AX + 14 + (int)lroundf((AW - 30) * (0.5f + 0.5f * sinf(u * 2 * PI_F)));
        int by = AY + 30 + (int)lroundf(10 * sinf(u * 6 * PI_F));
        int flap = ((now / 120) % 2) ? 4 : 2;
        g.fillEllipse(bx - 3 - ox, by - oy, 3, flap, C_WING);
        g.fillEllipse(bx + 3 - ox, by - oy, 3, flap, C_WING);
        g.drawFastVLine(bx - ox, by - 2 - oy, 5, C_DOOR);
    } else {                                                       // fireflies
        for (int i = 0; i < 4; i++) {
            int fx = AX + 12 + i * 22 + (int)lroundf(sinf(now / 700.0f + i) * 6);
            int fy = AY + 24 + (int)lroundf(cosf(now / 900.0f + i * 2) * 12);
            float b = 0.5f + 0.5f * sinf(now / 300.0f + i);
            g.fillCircle(fx - ox, fy - oy, 3, mix565(C_FIREFLY, sky, (uint8_t)(60 * b)));
            g.fillCircle(fx - ox, fy - oy, 1, mix565(C_FIREFLY, sky, (uint8_t)(120 + 135 * b)));
        }
    }
    anim.pushSprite(AX, AY);
}

// ---- clock + info line ------------------------------------------------------------

void drawRoomClock(const SceneInfo& s) {
    // New sky (weather changed)? Repaint the whole garden first.
    const Wx want = s.weatherOk ? wxOf(skyFromWmo(s.weatherCode)) : Wx::CLEAR;
    if (want != wx) {
        wx = want;
        sceneRepaintRect(0, HEADER_H, SCREEN_W, ROOM_STRIP_Y - HEADER_H);
    } else {
        sceneRepaintRect(0, HEADER_H, CLOCK_W, ROOM_CAT_Y - HEADER_H);
    }
    const uint16_t sky = skyColour();
    const int maxW = CLOCK_W - 2 * MARGIN;

    char buf[48];
    if (s.timeKnown) snprintf(buf, sizeof(buf), "%02d:%02d", s.hour, s.minute);
    else             strlcpy(buf, "--:--", sizeof(buf));
    drawTextCentre(buf, ROOM_CLOCK_CX, 30, UiFont::CLOCK, C_TEXT, sky);

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
    const uint16_t infoCol = mix565(C_TEXT, sky, 200);
    for (int i = 0; i < n; i++) {
        if (textWidthIn(cand[i], UiFont::SMALL) <= maxW || i == n - 1) {
            char fit[48];
            fitText(cand[i], fit, sizeof(fit), maxW, UiFont::SMALL);
            drawTextCentre(fit, ROOM_CLOCK_CX, 68, UiFont::SMALL, infoCol, sky);
            break;
        }
    }
}
