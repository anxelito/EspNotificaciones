// firmware/src/ui/room_scene.cpp -- the frog's pond
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
static constexpr uint16_t C_TREE     = rgb565(0x4c, 0x9a, 0x5a);
static constexpr uint16_t C_TREE2    = rgb565(0x3c, 0x84, 0x4c);
static constexpr uint16_t C_TRUNK    = rgb565(0x7a, 0x52, 0x34);
static constexpr uint16_t C_BANK     = rgb565(0x6c, 0xbc, 0x68);
static constexpr uint16_t C_BANK_D   = rgb565(0x58, 0xa4, 0x56);
static constexpr uint16_t C_WATER    = rgb565(0x3a, 0x9c, 0xc6);
static constexpr uint16_t C_WATER_D  = rgb565(0x2a, 0x84, 0xae);
static constexpr uint16_t C_WATER_L  = rgb565(0x8a, 0xd0, 0xec);
static constexpr uint16_t C_PAD      = rgb565(0x4c, 0xb8, 0x64);
static constexpr uint16_t C_PAD_D    = rgb565(0x38, 0x96, 0x4e);
static constexpr uint16_t C_REED     = rgb565(0x4a, 0x7a, 0x3a);
static constexpr uint16_t C_CATTAIL  = rgb565(0x7a, 0x4b, 0x2a);
static constexpr uint16_t C_LOTUS    = rgb565(0xff, 0x9a, 0xd0);
static constexpr uint16_t C_LOTUS_C  = rgb565(0xff, 0xe2, 0x7a);
static constexpr uint16_t C_DRAGON   = rgb565(0x3a, 0xb8, 0xd8);
static constexpr uint16_t C_WINGF    = rgb565(0xd8, 0xf0, 0xff);
static constexpr uint16_t C_FIREFLY  = rgb565(0xff, 0xf2, 0x7a);
static constexpr uint16_t C_BODYDK   = rgb565(0x1c, 0x24, 0x30);

static const int AX = 208, AY = 30, AW = 96, AH = 50;     // animated corner
static const int CLOCK_W = 170;                            // clock column x 0..170
static const int REEDS_X = 272;
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

// ---- static pond -----------------------------------------------------------------

static void lilyPad(TFT_eSPI& g, int x, int y, int rx, int ry, uint16_t sky) {
    g.fillEllipse(x, y, rx, ry, night(C_PAD));
    g.fillEllipse(x, y + 1, rx - 3, ry - 2, night(C_PAD_D));
    g.fillEllipse(x, y, rx - 3, ry - 2, night(C_PAD));
    g.fillTriangle(x, y, x + rx, y - 2, x + rx, y + 2, night(C_WATER));   // notch
    (void)sky;
}

static void reeds(TFT_eSPI& g, int ox, int oy) {
    static const int rx[] = { 0, 9, 20, 31, 40 };
    static const int rh[] = { 58, 70, 52, 64, 46 };
    for (int i = 0; i < 5; i++) {
        int x = REEDS_X + rx[i] - ox, base = ROOM_FLOOR_Y + 6 - oy, top = base - rh[i];
        g.fillRect(x, top, 2, base - top, night(C_REED));
        if (i % 2 == 0) g.fillRoundRect(x - 2, top - 2, 6, 15, 3, night(C_CATTAIL));
        else            g.fillTriangle(x - 5, top + 20, x + 1, top + 4, x + 1, top + 24, night(C_REED));
    }
    // Big pad with a lotus flower in front of the reeds.
    lilyPad(g, REEDS_X + 22 - ox, ROOM_FLOOR_Y + 8 - oy, 22, 6, 0);
    int fx = REEDS_X + 16 - ox, fy = ROOM_FLOOR_Y + 2 - oy;
    g.fillTriangle(fx - 6, fy + 2, fx, fy - 9, fx + 6, fy + 2, night(C_LOTUS));
    g.fillTriangle(fx - 10, fy + 3, fx - 5, fy - 5, fx, fy + 3, night(C_LOTUS));
    g.fillTriangle(fx, fy + 3, fx + 5, fy - 5, fx + 10, fy + 3, night(C_LOTUS));
    g.fillCircle(fx, fy, 2, night(C_LOTUS_C));
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

    // Trees behind the far bank.
    for (int i = 0; i < 8; i++) {
        int tx = 18 + i * 42 - ox, ty = 128 - (i % 3) * 6 - oy;
        g.fillRect(tx - 2, ty + 8, 4, 12, night(C_TRUNK));
        g.fillEllipse(tx, ty, 22, 16, night(i % 2 ? C_TREE : C_TREE2));
    }
    const bool snow = wx == Wx::SNOW;
    g.fillRect(-ox, 136 - oy, SCREEN_W, 14, night(snow ? rgb565(0xee, 0xf4, 0xfa) : C_BANK));
    g.drawFastHLine(-ox, 136 - oy, SCREEN_W, night(snow ? rgb565(0xc8, 0xd4, 0xe4) : C_BANK_D));

    // Pond (the frog's lily pad floats on it; the surface line is ROOM_FLOOR_Y).
    g.fillRect(-ox, 150 - oy, SCREEN_W, ROOM_STRIP_Y - 150, night(C_WATER));
    g.fillRect(-ox, ROOM_FLOOR_Y + 4 - oy, SCREEN_W, ROOM_STRIP_Y - ROOM_FLOOR_Y - 4, night(C_WATER_D));
    g.drawFastHLine(-ox, 150 - oy, SCREEN_W, night(C_WATER_L));
    for (int i = 0; i < 12; i++) {
        int wx0 = (i * 53 + 17) % SCREEN_W, wy = 154 + (i * 7) % 26;
        g.drawFastHLine(wx0 - ox, wy - oy, 8 + (i % 3) * 3, night(C_WATER_L));
    }
    lilyPad(g, 30 - ox, 158 - oy, 14, 4, 0);
    lilyPad(g, 214 - ox, 156 - oy, 12, 3, 0);
    if (wx == Wx::RAIN) {                                   // rain rings on the water
        for (int i = 0; i < 6; i++)
            g.drawEllipse((i * 61 + 25) % 260 - ox, 160 + (i * 11) % 20 - oy, 5, 1, night(C_WATER_L));
    }
    reeds(g, ox, oy);
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
    } else if (!lookNight) {                                       // dragonfly
        float u = (float)(now % 7000) / 7000.0f;
        int bx = AX + 12 + (int)lroundf((AW - 66) * (0.5f + 0.5f * sinf(u * 2 * PI_F)));   // left of the sun
        int by = AY + 28 + (int)lroundf(8 * sinf(u * 8 * PI_F));
        int dir = cosf(u * 2 * PI_F) > 0 ? 1 : -1;
        bool up = (now / 60) % 2;
        g.fillEllipse(bx - 3 - ox, by - (up ? 3 : 1) - oy, 5, 2, C_WINGF);
        g.fillEllipse(bx + 3 - ox, by - (up ? 3 : 1) - oy, 5, 2, C_WINGF);
        g.fillRect((dir > 0 ? bx - 9 : bx - 5) - ox, by - oy, 14, 2, C_DRAGON);
        g.fillCircle(bx + dir * 6 - ox, by - oy, 2, C_BODYDK);
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
    // New sky (weather changed)? Repaint the whole scene first.
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
