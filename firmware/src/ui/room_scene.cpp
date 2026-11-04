// firmware/src/ui/room_scene.cpp -- space station room
#include "room_scene.h"
#include <math.h>
#include "theme.h"
#include "fonts.h"
#include "text_fit.h"
#include "screen_room.h"
#include "../display.h"

// ---- palette ----------------------------------------------------------------
static constexpr uint16_t C_WALL    = rgb565(0x0c, 0x10, 0x20);
static constexpr uint16_t C_WALL_L  = rgb565(0x14, 0x19, 0x2e);
static constexpr uint16_t C_FLOOR   = rgb565(0x1a, 0x20, 0x36);
static constexpr uint16_t C_SEAM    = rgb565(0x2a, 0x33, 0x52);
static constexpr uint16_t C_SPACE   = rgb565(0x04, 0x06, 0x0f);
static constexpr uint16_t C_RIM     = rgb565(0x3b, 0x44, 0x60);
static constexpr uint16_t C_BOLT    = rgb565(0x7a, 0x86, 0xa8);
static constexpr uint16_t C_STAR    = rgb565(0xdd, 0xdd, 0xee);
static constexpr uint16_t C_STAR_D  = rgb565(0x66, 0x66, 0x88);
static constexpr uint16_t C_PANEL   = rgb565(0x1f, 0x27, 0x42);
static constexpr uint16_t C_LED_OFF = rgb565(0x33, 0x40, 0x5f);
static constexpr uint16_t C_PLANET  = rgb565(0xb4, 0x8c, 0xff);   // comfort OK

static const int WIN_CX = 222, WIN_CY = 96, WIN_R = 58;
static const int PANEL_X = 291, PANEL_Y = 104, PANEL_W = 22, PANEL_H = 56;

static bool    lookNight   = false;
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

void sceneInit() {}

void sceneSetLook(bool night, Comfort comfort) {
    lookNight = night;
    lookComfort = comfort;
}

static uint16_t planetColour() {
    switch (lookComfort) {
        case Comfort::COLD: return C_COLD;
        case Comfort::HOT:  return C_HOT;
        default:            return C_PLANET;
    }
}

// Window: space, stars, ringed planet, rim with bolts. Static.
static void window(TFT_eSPI& g, int ox, int oy) {
    const int cx = WIN_CX - ox, cy = WIN_CY - oy;
    g.fillCircle(cx, cy, WIN_R, C_SPACE);

    // Stars inside the circle (fixed pseudo-random field).
    for (int i = 0; i < 46; i++) {
        int sx = (i * 53 + 11) % (2 * WIN_R) - WIN_R;
        int sy = (i * 37 + 5) % (2 * WIN_R) - WIN_R;
        if (sx * sx + sy * sy > (WIN_R - 3) * (WIN_R - 3)) continue;
        g.drawPixel(cx + sx, cy + sy, (i % 4 == 0) ? C_STAR_D : C_STAR);
        if (i % 9 == 0) g.drawPixel(cx + sx + 1, cy + sy, C_STAR);
    }

    // Planet: lit side + shadow (darker at night), ring in front.
    const uint16_t pl = planetColour();
    const int px = cx + 14, py = cy + 16;
    const uint16_t lit = lookNight ? mix565(pl, 0x0000, 140) : pl;
    g.fillCircle(px, py, 25, lit);
    g.fillCircle(px + 8, py - 7, 24, mix565(pl, 0x0000, lookNight ? 60 : 110));
    g.fillCircle(px - 9, py - 8, 3, mix565(0xFFFF, lit, 80));        // crater / shine
    const uint16_t ring = mix565(pl, 0xFFFF, lookNight ? 110 : 150);
    g.drawEllipse(px, py, 42, 9, ring);
    g.drawEllipse(px, py + 1, 42, 9, ring);
    if (!lookNight) {                                              // distant sun glare
        g.fillCircle(cx - 32, cy - 28, 5, rgb565(0xff, 0xe6, 0x9a));
        g.drawCircle(cx - 32, cy - 28, 8, rgb565(0x80, 0x73, 0x4d));
    }

    // Rim: thick ring + bolts.
    for (int r = WIN_R; r < WIN_R + 6; r++) g.drawCircle(cx, cy, r, C_RIM);
    for (int a = 0; a < 8; a++) {
        float t = a * 3.14159265f / 4.0f;
        g.fillCircle(cx + (int)lroundf(cosf(t) * (WIN_R + 3)),
                     cy + (int)lroundf(sinf(t) * (WIN_R + 3)), 2, C_BOLT);
    }
}

void drawSceneLower(TFT_eSPI& g, int ox, int oy) {
    // Wall with two faint panel seams.
    g.fillRect(-ox, HEADER_H - oy, SCREEN_W, ROOM_FLOOR_Y - HEADER_H, C_WALL);
    g.drawFastHLine(-ox, 112 - oy, SCREEN_W, C_WALL_L);
    g.drawFastHLine(-ox, 140 - oy, SCREEN_W, C_WALL_L);
    window(g, ox, oy);
    // Control panel housing (lights are animated in drawSceneAnim).
    g.fillRoundRect(PANEL_X - ox, PANEL_Y - oy, PANEL_W, PANEL_H, 4, C_PANEL);
    // Metal floor with seams.
    g.fillRect(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, ROOM_STRIP_Y - ROOM_FLOOR_Y, C_FLOOR);
    g.drawFastHLine(-ox, ROOM_FLOOR_Y - oy, SCREEN_W, C_SEAM);
    g.drawFastHLine(-ox, ROOM_FLOOR_Y + 1 - oy, SCREEN_W, C_SEAM);
    for (int x = 0; x < SCREEN_W; x += 32)
        g.drawFastVLine(x - ox, ROOM_FLOOR_Y + 2 - oy, ROOM_STRIP_Y - ROOM_FLOOR_Y - 2, C_SEAM);
}

void sceneRepaintRect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.setViewport(x, y, w, h, false);   // absolute coords, clipped to the rect
    drawSceneLower(tft, 0, 0);
    tft.resetViewport();
}

void drawSceneAnim(uint32_t now) {
    // 2 x 4 lights chasing; one shows the comfort colour.
    const uint16_t on[] = { C_OK, C_COLD, C_OK, C_HOT };
    int step = (int)(now / 280) % 8;
    for (int i = 0; i < 8; i++) {
        int col = i % 2, row = i / 2;
        int x = PANEL_X + 7 + col * 8, y = PANEL_Y + 9 + row * 12;
        bool lit = (i == step) || ((i + 3) % 8 == step) || (i == 7 && (now / 900) % 2);
        tft.fillCircle(x, y, 2, lit ? on[i % 4] : C_LED_OFF);
    }
}

// ---- clock + info line -----------------------------------------------------------

void drawRoomClock(const SceneInfo& s) {
    const int x0 = 0, x1 = WIN_CX - WIN_R - 8;            // left of the window rim
    const int maxW = x1 - x0 - 2 * MARGIN;
    tft.fillRect(x0, HEADER_H + 1, x1 - x0, ROOM_CAT_Y - HEADER_H - 1, C_WALL);

    char buf[48];
    if (s.timeKnown) snprintf(buf, sizeof(buf), "%02d:%02d", s.hour, s.minute);
    else             strlcpy(buf, "--:--", sizeof(buf));
    drawTextCentre(buf, ROOM_CLOCK_CX, 30, UiFont::CLOCK, C_TEXT, C_WALL);

    static const char* const DAYS[] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
    static const char* const MONTHS[] = { "ene", "feb", "mar", "abr", "may", "jun",
                                          "jul", "ago", "sep", "oct", "nov", "dic" };
    const bool haveDate = s.timeKnown && s.wday >= 0 && s.wday < 7 && s.mon >= 0 && s.mon < 12;
    const char* day = haveDate ? DAYS[s.wday] : "";
    const char* mon = haveDate ? MONTHS[s.mon] : "";
    const int t = (int)lroundf(s.outTemp);
    const char* word = s.weatherOk ? skyWord(skyFromWmo(s.weatherCode)) : "";

    // Longest variant that fits the column: full, no month, no sky word, ...
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
    for (int i = 0; i < n; i++) {
        if (textWidthIn(cand[i], UiFont::SMALL) <= maxW || i == n - 1) {
            char fit[48];
            fitText(cand[i], fit, sizeof(fit), maxW, UiFont::SMALL);   // last resort
            drawTextCentre(fit, ROOM_CLOCK_CX, 68, UiFont::SMALL, C_MUTED, C_WALL);
            break;
        }
    }
}
