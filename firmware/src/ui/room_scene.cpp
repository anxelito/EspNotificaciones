// firmware/src/ui/room_scene.cpp -- the penguin's ice field
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
static constexpr uint16_t C_ICE      = rgb565(0xd8, 0xec, 0xfa);
static constexpr uint16_t C_ICE2     = rgb565(0xb4, 0xd6, 0xf0);
static constexpr uint16_t C_ICE_L    = rgb565(0xff, 0xff, 0xff);
static constexpr uint16_t C_SNOWFLD  = rgb565(0xee, 0xf6, 0xff);
static constexpr uint16_t C_SNOW_D   = rgb565(0xc4, 0xdc, 0xf0);
static constexpr uint16_t C_SEA      = rgb565(0x2a, 0x6c, 0xb0);
static constexpr uint16_t C_SEA_L    = rgb565(0x5a, 0x9c, 0xd8);
static constexpr uint16_t C_IGLOO    = rgb565(0xf6, 0xfb, 0xff);
static constexpr uint16_t C_IGLOO_D  = rgb565(0xc4, 0xd8, 0xec);
static constexpr uint16_t C_DOOR     = rgb565(0x1c, 0x26, 0x38);
static constexpr uint16_t C_PUDDLE   = rgb565(0x8c, 0xc4, 0xec);
static constexpr uint16_t C_AUR1     = rgb565(0x5a, 0xff, 0xb4);
static constexpr uint16_t C_AUR2     = rgb565(0xa6, 0x7a, 0xff);
static constexpr uint16_t C_BIRD     = rgb565(0x2a, 0x30, 0x44);
static constexpr uint16_t C_FLAG     = rgb565(0xff, 0x5c, 0x5c);

static const int AX = 208, AY = 30, AW = 96, AH = 50;     // animated corner
static const int CLOCK_W = 170;                            // clock column x 0..170
static const int IGLOO_CX = 286;
static const float PI_F = 3.14159265f;

static TFT_eSprite anim(&tft);
static bool haveAnim = false;

static bool    lookNight = false;
static Comfort lookComfort = Comfort::UNKNOWN;   // hot room: the snow melts
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

void sceneSetLook(bool night, Comfort comfort) { lookNight = night; lookComfort = comfort; }

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

// ---- static ice field --------------------------------------------------------------

static void igloo(TFT_eSPI& g, int ox, int oy) {
    const int cx = IGLOO_CX - ox, base = ROOM_FLOOR_Y + 2 - oy;
    g.fillCircle(cx, base, 34, night(C_IGLOO));
    g.fillRect(cx - 35, base, 70, 12, night(C_SNOWFLD));
    // Block lines: rings + staggered joints.
    for (int r = 12; r <= 34; r += 11) g.drawCircle(cx, base, r, night(C_IGLOO_D));
    for (int i = 0; i < 7; i++) {
        float a = PI_F * (0.08f + i * 0.14f);
        g.drawLine(cx + (int)(cosf(a) * 23), base - (int)(sinf(a) * 23),
                   cx + (int)(cosf(a) * 34), base - (int)(sinf(a) * 34), night(C_IGLOO_D));
    }
    g.fillRect(cx - 35, base + 1, 70, 12, night(C_SNOWFLD));
    // Entrance tunnel facing left.
    g.fillRoundRect(cx - 46, base - 22, 28, 24, 10, night(C_IGLOO));
    g.drawRoundRect(cx - 46, base - 22, 28, 24, 10, night(C_IGLOO_D));
    g.fillRoundRect(cx - 41, base - 16, 18, 18, 8, C_DOOR);
    g.fillRect(cx - 46, base + 1, 30, 12, night(C_SNOWFLD));
    // Little flag on top.
    g.drawFastVLine(cx + 4, base - 50, 17, night(C_BIRD));
    g.fillTriangle(cx + 5, base - 50, cx + 5, base - 43, cx + 14, base - 47, night(C_FLAG));
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
        if (wx == Wx::CLEAR || wx == Wx::PARTLY) {            // still aurora low in the sky
            for (int k = 0; k < 2; k++) {
                uint16_t c = mix565(k ? C_AUR2 : C_AUR1, sky, 90);
                for (int x = CLOCK_W - 30; x < SCREEN_W; x += 2) {
                    int y = 92 + k * 8 + (int)lroundf(4 * sinf(x / 18.0f + k * 1.7f));
                    g.drawFastVLine(x - ox, y - oy, 4, c);
                    g.drawFastVLine(x + 1 - ox, y - oy, 4, c);
                }
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

    // Ice mountains and a strip of sea.
    g.fillTriangle(-30 - ox, 148 - oy, 50 - ox, 92 - oy, 130 - ox, 148 - oy, night(C_ICE2));
    g.fillTriangle(32 - ox, 105 - oy, 50 - ox, 92 - oy, 66 - ox, 104 - oy, night(C_ICE_L));
    g.fillTriangle(100 - ox, 148 - oy, 188 - ox, 104 - oy, 270 - ox, 148 - oy, night(C_ICE));
    g.fillTriangle(173 - ox, 112 - oy, 188 - ox, 104 - oy, 202 - ox, 111 - oy, night(C_ICE_L));
    g.fillTriangle(220 - ox, 148 - oy, 290 - ox, 114 - oy, 350 - ox, 148 - oy, night(C_ICE2));
    g.fillRect(-ox, 146 - oy, SCREEN_W, 10, night(C_SEA));
    for (int i = 0; i < 9; i++)
        g.drawFastHLine((i * 41 + 9) % SCREEN_W - ox, 148 + (i % 3) * 2 - oy, 10, night(C_SEA_L));

    // Snow field (meltwater puddles when the room is hot).
    g.fillRect(-ox, 156 - oy, SCREEN_W, ROOM_STRIP_Y - 156, night(C_SNOWFLD));
    g.drawFastHLine(-ox, 156 - oy, SCREEN_W, night(C_SNOW_D));
    for (int i = 0; i < 14; i++) {
        int tx = (i * 53 + 11) % SCREEN_W, ty = ROOM_FLOOR_Y + 3 + (i * 7) % 12;
        g.drawFastHLine(tx - ox, ty - oy, 6, night(C_SNOW_D));
    }
    if (lookComfort == Comfort::HOT) {
        g.fillEllipse(70 - ox, ROOM_FLOOR_Y + 9 - oy, 20, 3, night(C_PUDDLE));
        g.fillEllipse(200 - ox, ROOM_FLOOR_Y + 12 - oy, 15, 2, night(C_PUDDLE));
    }
    igloo(g, ox, oy);
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
    } else if (!lookNight) {                                       // seabird gliding
        float u = (float)(now % 8000) / 8000.0f;
        int bx = AX + 10 + (int)lroundf((AW - 24) * u);
        int by = AY + 30 + (int)lroundf(6 * sinf(u * 4 * PI_F));
        int flap = ((now / 160) % 2) ? 3 : -1;
        for (int k = 0; k < 2; k++) {
            g.drawLine(bx - 6 - ox, by - flap + k - oy, bx - ox, by + k - oy, C_BIRD);
            g.drawLine(bx - ox, by + k - oy, bx + 6 - ox, by - flap + k - oy, C_BIRD);
        }
    }
    if (lookNight && (wx == Wx::CLEAR || wx == Wx::PARTLY)) {     // aurora ripples
        for (int k = 0; k < 2; k++) {
            uint16_t c = mix565(k ? C_AUR2 : C_AUR1, sky, 120);
            for (int x = AX; x < AX + AW; x += 2) {
                int y = AY + 10 + k * 10 + (int)lroundf(5 * sinf(x / 14.0f + now / 700.0f + k * 1.7f));
                g.drawFastVLine(x - ox, y - oy, 5, c);
                g.drawFastVLine(x + 1 - ox, y - oy, 5, c);
            }
        }
        g.fillCircle(276 - ox, 50 - oy, 11, C_MOON);                 // moon in front
        g.fillCircle(282 - ox, 45 - oy, 10, sky);
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
