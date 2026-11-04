// firmware/src/ui/cat_draw.cpp -- the mascot: a little hovering robot
//
// (File and type names still say "cat": the mascot logic -- moods, idle acts,
// pose -- is shared; only the drawing changed.) Every frame is painted into
// one CAT_W x CAT_H sprite whose background is the room scene behind it, so
// pushSprite never flickers and the robot never shows a box.
//
// Idle acts: WALK = fly across, LOOK = eyes glance aside, GROOM = scan line,
// YAWN = low-battery blink, STRETCH = arms up, HOP = thruster bounce.
// Moods: PETTED = heart eyes + hop, WOKEN = sleepy eyes, SLEEPING = docked
// and charging. Comfort: eyes/antenna/chest light colour; cold = frost and
// shiver, hot = steam; humid = water drop; dry = spark.
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"
#include "room_scene.h"
#include "screen_room.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_METAL   = rgb565(0xc9, 0xd2, 0xe6);
static constexpr uint16_t C_METAL_D = rgb565(0x9a, 0xa6, 0xc2);
static constexpr uint16_t C_METAL_L = rgb565(0xe9, 0xee, 0xf8);
static constexpr uint16_t C_SCREEN  = rgb565(0x1a, 0x20, 0x36);
static constexpr uint16_t C_FLAME   = rgb565(0x7a, 0xd7, 0xff);
static constexpr uint16_t C_FLAME_C = rgb565(0xe6, 0xf8, 0xff);
static constexpr uint16_t C_HEART   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_DROP    = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_ZZZ     = rgb565(0x8f, 0xa3, 0xff);
static constexpr uint16_t C_PAPER   = rgb565(0xf4, 0xf1, 0xe8);
static constexpr uint16_t C_SHADOW  = rgb565(0x08, 0x0b, 0x16);
static constexpr uint16_t C_DOCK    = rgb565(0x2a, 0x33, 0x52);
static constexpr uint16_t C_STEAM   = rgb565(0xd8, 0xdd, 0xe8);
static constexpr uint16_t C_SPARK   = rgb565(0xff, 0xe8, 0x6a);

static const float PI_F = 3.14159265f;
// Floor line inside the sprite (shadow / dock sit here).
static const int FLOOR_IN = ROOM_FLOOR_Y - ROOM_CAT_Y;   // 78

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[bot] sprite alloc failed, drawing direct");
    return haveSprite;
}

// ---- small parts -------------------------------------------------------------

static void line2(TFT_eSPI& g, int x0, int y0, int x1, int y1, uint16_t c) {
    g.drawLine(x0, y0, x1, y1, c);
    g.drawLine(x0 + 1, y0, x1 + 1, y1, c);
    g.drawLine(x0, y0 + 1, x1, y1 + 1, c);
}

static void heart(TFT_eSPI& g, int x, int y, int s, uint16_t c) {
    g.fillCircle(x - s, y, s, c);
    g.fillCircle(x + s, y, s, c);
    g.fillTriangle(x - 2 * s, y + 1, x + 2 * s, y + 1, x, y + 2 * s + 1, c);
}

static void drop(TFT_eSPI& g, int x, int y) {
    g.fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y - 3, C_DROP);
    g.fillCircle(x, y + 4, 3, C_DROP);
}

static float bump(float ph) { return sinf(ph * PI_F); }
static float hold(float ph) {
    if (ph < 0.15f) return ph / 0.15f;
    if (ph > 0.85f) return (1.0f - ph) / 0.15f;
    return 1.0f;
}

// ---- awake ---------------------------------------------------------------------

static void drawAwake(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint16_t glow = p.body;                     // comfort colour
    const bool idle = p.mood == CatMood::IDLE;
    const CatAct act = idle ? p.act : CatAct::SIT;
    const float ph = p.actPhase;
    const int face = p.facing >= 0 ? 1 : -1;
    const uint32_t t = p.nowMs;

    // Cold: shiver by a pixel while hovering in place.
    int shiver = 0;
    if (idle && act == CatAct::SIT && p.comfort == Comfort::COLD)
        shiver = ((t / 70) % 2) ? 1 : -1;
    const int cx = ox + CAT_W / 2 + shiver;

    // Height: hover bob, HOP / petting bounce.
    int lift = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = t - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        lift = (int)lroundf(6.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        lift = (int)lroundf(6.0f * bump(ph));
    }
    const int bob = (int)lroundf(2.5f * sinf((float)(t % 1400) * (2.0f * PI_F / 1400.0f)));
    const int hy = oy + 20 + bob - lift;              // top of the head
    const int tilt = (act == CatAct::WALK) ? face * 2 : 0;   // lean into the flight

    // Shadow on the floor, smaller when higher.
    int sh = 17 - lift / 2 - bob;
    g.fillEllipse(cx, oy + FLOOR_IN + 6, sh, 3, C_SHADOW);

    // Thruster flame under the body (bigger while flying / hopping).
    {
        int len = 5 + (int)((t / 60) % 3);
        if (act == CatAct::WALK || act == CatAct::HOP || p.mood == CatMood::PETTED) len += 4;
        int fy = hy + 60;
        g.fillTriangle(cx - 7, fy, cx + 7, fy, cx - tilt, fy + len, C_FLAME);
        g.fillTriangle(cx - 3, fy, cx + 3, fy, cx - tilt, fy + len - 4, C_FLAME_C);
    }

    // Antenna with a glowing tip; spins faster while scanning.
    {
        float sp = (act == CatAct::GROOM) ? 120.0f : 380.0f;
        int sway = (int)lroundf(4.0f * sinf((float)t / sp));
        int ax = cx + tilt + sway, ay = hy - 8;
        line2(g, cx + tilt, hy, ax, ay, C_METAL_D);
        g.fillCircle(ax, ay, 5, mix565(glow, C_SCREEN, 70));   // glow halo
        g.fillCircle(ax, ay, 3, glow);
        g.drawPixel(ax - 1, ay - 2, C_METAL_L);
    }

    // Arms (behind the body): down by default, up while stretching, one
    // waving while petted.
    const float st = (act == CatAct::STRETCH) ? bump(ph) : 0.0f;
    {
        int sy = hy + 44;
        int down = 14 - (int)(26 * st);
        int lhx = cx - 26 - (int)(4 * st), lhy = sy + down;
        int rhx = cx + 26 + (int)(4 * st), rhy = sy + down;
        if (p.mood == CatMood::PETTED) rhy = sy - 8 + (int)(4 * sinf(t / 90.0f));
        if (act == CatAct::WALK) { lhy -= face > 0 ? 4 : 0; rhy -= face < 0 ? 4 : 0; }
        line2(g, cx - 16, sy, lhx, lhy, C_METAL_D);
        line2(g, cx + 15, sy, rhx, rhy, C_METAL_D);
        g.fillCircle(lhx, lhy, 4, C_METAL);
        g.fillCircle(rhx, rhy, 4, C_METAL);
    }

    // Body + chest light.
    g.fillRoundRect(cx - 17 + tilt / 2, hy + 38, 34, 22, 9, C_METAL_D);
    g.fillRoundRect(cx - 15 + tilt / 2, hy + 39, 30, 6, 3, C_METAL);
    {
        float pulse = 0.5f + 0.5f * sinf((float)(t % 1600) * (2.0f * PI_F / 1600.0f));
        g.fillCircle(cx + tilt / 2, hy + 50, 5, mix565(glow, C_METAL_D, (uint8_t)(120 + 135 * pulse)));
    }

    // Head + screen face.
    const int hx = cx - 24 + tilt;
    g.fillRoundRect(hx, hy, 48, 36, 11, C_METAL);
    g.fillRoundRect(hx + 3, hy + 2, 42, 4, 2, C_METAL_L);            // shine
    g.fillRoundRect(hx + 6, hy + 7, 36, 23, 7, C_SCREEN);
    g.fillCircle(hx - 1, hy + 18, 3, C_METAL_D);                       // ear bolts
    g.fillCircle(hx + 49, hy + 18, 3, C_METAL_D);

    // Eyes on the screen.
    int pdx = 0, pdy = 0;
    if (act == CatAct::WALK) pdx = 3 * face;
    if (act == CatAct::LOOK) { pdx = (int)lroundf(5.0f * p.look * hold(ph)); pdy = -1; }
    const int ey = hy + 12 + pdy;
    const int exL = hx + 13 + pdx, exR = hx + 27 + pdx;
    const bool sleepy = p.mood == CatMood::WOKEN;
    const bool lowBat = act == CatAct::YAWN;
    if (p.mood == CatMood::PETTED) {
        heart(g, exL + 4, ey + 3, 2, C_HEART);
        heart(g, exR + 4, ey + 3, 2, C_HEART);
    } else if (act == CatAct::GROOM) {                 // scanning line sweeps
        int sx = hx + 9 + (int)((ph * 7.0f - floorf(ph * 7.0f)) * 28.0f);
        g.fillRect(hx + 9, ey + 4, 30, 2, mix565(glow, C_SCREEN, 90));
        g.fillRect(sx, ey + 1, 3, 8, glow);
    } else if (lowBat) {                               // battery icon blinking
        bool on = ((t / 250) % 2) == 0;
        g.drawRect(hx + 15, ey, 16, 9, glow);
        g.fillRect(hx + 31, ey + 3, 2, 3, glow);
        if (on) g.fillRect(hx + 17, ey + 2, 3, 5, C_URGENT);
    } else if (p.blink || st > 0.3f) {
        g.fillRect(exL, ey + 4, 8, 2, glow);
        g.fillRect(exR, ey + 4, 8, 2, glow);
    } else if (sleepy) {
        g.fillRoundRect(exL, ey + 3, 8, 5, 2, glow);
        g.fillRoundRect(exR, ey + 3, 8, 5, 2, glow);
    } else {
        g.fillRoundRect(exL, ey, 8, 10, 3, glow);
        g.fillRoundRect(exR, ey, 8, 10, 3, glow);
        g.fillRect(exL + 1, ey + 1, 2, 2, C_METAL_L);
        g.fillRect(exR + 1, ey + 1, 2, 2, C_METAL_L);
    }
    // Mouth: small smile, open "o" when hot.
    if (!lowBat && act != CatAct::GROOM) {
        if (p.comfort == Comfort::HOT) g.fillCircle(hx + 24 + pdx / 2, hy + 25, 2, glow);
        else g.drawFastHLine(hx + 20 + pdx / 2, hy + 25, 8, mix565(glow, C_SCREEN, 160));
    }

    // Comfort / humidity extras.
    if (p.comfort == Comfort::COLD) {                  // frost on the head corners
        g.fillTriangle(hx + 2, hy + 3, hx + 9, hy + 1, hx + 3, hy + 8, C_METAL_L);
        g.fillTriangle(hx + 46, hy + 3, hx + 39, hy + 1, hx + 45, hy + 8, C_METAL_L);
        if ((t / 700) % 4 == 0) { g.drawCircle(hx + 52, hy + 24, 2, C_DROP); g.drawCircle(hx + 57, hy + 20, 1, C_DROP); }
    } else if (p.comfort == Comfort::HOT) {            // steam puffs rising
        for (int i = 0; i < 2; i++) {
            float sp = (float)((t + i * 600) % 1200) / 1200.0f;
            g.fillCircle(hx + 10 + i * 28, hy - 2 - (int)(sp * 12), 2 + (int)(sp * 2),
                         mix565(C_STEAM, C_SCREEN, (uint8_t)(220 * (1.0f - sp))));
        }
    }
    if (p.humid == HumidMood::HUMID) drop(g, hx + 50, hy + 4 + (int)((t / 150) % 6));
    if (p.humid == HumidMood::DRY && (t / 400) % 5 == 0) {
        g.drawLine(hx - 4, hy + 30, hx - 1, hy + 26, C_SPARK);
        g.drawLine(hx - 1, hy + 26, hx - 5, hy + 25, C_SPARK);
    }

    // Letter held up (new task), floating heart (petted).
    if (p.envelope) {
        int ex = cx - 10, eyy = hy + 44 + (int)((t / 300) % 2);
        g.fillRect(ex, eyy, 20, 13, C_PAPER);
        g.drawRect(ex, eyy, 20, 13, C_METAL_D);
        g.drawLine(ex, eyy, ex + 10, eyy + 7, C_METAL_D);
        g.drawLine(ex + 19, eyy, ex + 10, eyy + 7, C_METAL_D);
        g.fillCircle(ex + 10, eyy + 7, 2, C_HEART);
    }
    if (p.mood == CatMood::PETTED) heart(g, cx + 34, oy + 22 - (int)(petPhase * 14.0f), 3, C_HEART);
}

// ---- asleep: docked and charging -------------------------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint32_t t = p.nowMs;
    const int cx = ox + CAT_W / 2;
    const uint16_t glow = mix565(p.body, C_SCREEN, 110);
    const uint16_t metal = mix565(C_METAL, 0x0000, 150);
    const uint16_t metalD = mix565(C_METAL_D, 0x0000, 150);

    // Charging dock on the floor, light pulsing green.
    const int dy = oy + FLOOR_IN;
    g.fillRoundRect(cx - 30, dy - 2, 60, 10, 4, C_DOCK);
    float pulse = 0.5f + 0.5f * sinf((float)(t % 2000) * (2.0f * PI_F / 2000.0f));
    g.fillRect(cx - 20, dy + 3, 40, 2, mix565(C_OK, C_DOCK, (uint8_t)(80 + 175 * pulse)));

    // Robot sitting on the dock: body, head, antenna drooped.
    const int hy = dy - 50;
    g.fillRoundRect(cx - 17, hy + 32, 34, 18, 8, metalD);
    g.fillCircle(cx - 22, hy + 44, 4, metal);
    g.fillCircle(cx + 22, hy + 44, 4, metal);
    line2(g, cx, hy, cx + 9, hy - 6, metalD);
    g.fillCircle(cx + 10, hy - 7, 3, glow);
    g.fillRoundRect(cx - 24, hy, 48, 34, 11, metal);
    g.fillRoundRect(cx - 18, hy + 6, 36, 22, 7, C_SCREEN);
    g.fillRect(cx - 11, hy + 17, 8, 2, glow);          // closed eyes
    g.fillRect(cx + 3, hy + 17, 8, 2, glow);

    // Battery filling up, next to the head.
    int lvl = (int)((t / 600) % 5);
    g.drawRect(cx + 28, hy + 4, 14, 8, C_OK);
    g.fillRect(cx + 42, hy + 6, 2, 4, C_OK);
    for (int i = 0; i < lvl; i++) g.fillRect(cx + 30 + i * 2 + i / 2, hy + 6, 2, 4, C_OK);

    // "Zzz": three letters on a 3 s loop, rising and fading.
    const uint32_t ZP = 3000;
    for (int i = 0; i < 3; i++) {
        float zp = (float)((t + i * (ZP / 3)) % ZP) / ZP;
        float a  = zp < 0.15f ? zp / 0.15f : (1.0f - zp) / 0.85f;
        int zx = cx - 36 + (int)(zp * 14.0f);
        int zy = oy + 30 - (int)(zp * 28.0f);
        g.setTextColor(mix565(C_ZZZ, C_SCREEN, (uint8_t)(a * 255.0f)));
        g.drawString(zp < 0.66f ? "z" : "Z", zx, zy, zp < 0.33f ? 1 : 2);
    }
}

// ---- entry point -------------------------------------------------------------------

void catDraw(const CatPose& p, int x, int y) {
    TFT_eSPI& g = haveSprite ? (TFT_eSPI&)spr : tft;
    int ox = haveSprite ? 0 : x;
    int oy = haveSprite ? 0 : y;
    // Room behind the robot (wall, window, floor), so it never shows a box.
    if (haveSprite) drawSceneLower(spr, x, y);
    else            sceneRepaintRect(x, y, CAT_W, CAT_H);
    if (p.mood == CatMood::SLEEPING) drawAsleep(g, p, ox, oy);
    else                             drawAwake(g, p, ox, oy);
    if (haveSprite) spr.pushSprite(x, y);
}
