// firmware/src/ui/cat_draw.cpp -- the mascot: a penguin on the ice
//
// (File and type names still say "cat": the mascot logic -- moods, idle acts,
// pose -- is shared by every design; only the drawing changes.) Each frame is
// painted into one CAT_W x CAT_H sprite whose background is the ice field, so
// the penguin never flickers or shows a box.
//
// Idle acts: WALK = waddles, LOOK = turns its head, GROOM = preens a flipper,
// YAWN = beak wide open, STRETCH = flippers up on tiptoe, HOP = jumps with
// flippers out. Moods: PETTED = happy eyes, flapping, hearts; WOKEN = sleepy
// eyes; SLEEPING = sits down in a night cap. Comfort: the scarf shows the
// room colour; cold = happy sparkles (it loves it), hot = sweat drops,
// fanning itself with a flipper; humid = water drop.
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"
#include "room_scene.h"
#include "screen_room.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_BACK    = rgb565(0x24, 0x2a, 0x3c);
static constexpr uint16_t C_BACK_L  = rgb565(0x3a, 0x42, 0x5a);
static constexpr uint16_t C_BELLY   = rgb565(0xfa, 0xfa, 0xfa);
static constexpr uint16_t C_BELLY_D = rgb565(0xdc, 0xe4, 0xee);
static constexpr uint16_t C_BEAK    = rgb565(0xff, 0xb4, 0x3a);
static constexpr uint16_t C_BEAK_D  = rgb565(0xd8, 0x84, 0x22);
static constexpr uint16_t C_EYE     = rgb565(0x14, 0x16, 0x1e);
static constexpr uint16_t C_CHEEK   = rgb565(0xff, 0x9a, 0xb0);
static constexpr uint16_t C_HEART   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_DROP    = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_ZZZ     = rgb565(0xe8, 0xec, 0xff);
static constexpr uint16_t C_PAPER   = rgb565(0xf4, 0xf1, 0xe8);
static constexpr uint16_t C_SHADOW  = rgb565(0xb4, 0xcc, 0xe4);
static constexpr uint16_t C_SPARKLE = rgb565(0xe6, 0xf6, 0xff);
static constexpr uint16_t C_CAP     = rgb565(0x4a, 0x5f, 0xc8);
static constexpr uint16_t C_CAP_B   = rgb565(0xe8, 0xec, 0xf6);

static const float PI_F = 3.14159265f;
static const int FLOOR_IN = ROOM_FLOOR_Y - ROOM_CAT_Y;   // ground line inside the sprite

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[penguin] sprite alloc failed, drawing direct");
    return haveSprite;
}

// ---- small parts ---------------------------------------------------------------

static void heart(TFT_eSPI& g, int x, int y, int s, uint16_t c) {
    g.fillCircle(x - s, y, s, c);
    g.fillCircle(x + s, y, s, c);
    g.fillTriangle(x - 2 * s, y + 1, x + 2 * s, y + 1, x, y + 2 * s + 1, c);
}

static void drop(TFT_eSPI& g, int x, int y) {
    g.fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y - 3, C_DROP);
    g.fillCircle(x, y + 4, 3, C_DROP);
}

static void sparkle(TFT_eSPI& g, int x, int y, int r, uint16_t c) {
    g.drawFastHLine(x - r, y, 2 * r + 1, c);
    g.drawFastVLine(x, y - r, 2 * r + 1, c);
}

static float bump(float ph) { return sinf(ph * PI_F); }
static float hold(float ph) {
    if (ph < 0.15f) return ph / 0.15f;
    if (ph > 0.85f) return (1.0f - ph) / 0.15f;
    return 1.0f;
}

// Flipper: a slim ellipse hanging from the shoulder at angle a (0 = down,
// +PI/2 = straight out to the side given by s).
static void flipper(TFT_eSPI& g, int sx, int sy, int s, float a, uint16_t c) {
    for (int i = 0; i <= 14; i += 2) {
        int x = sx + s * (int)lroundf(sinf(a) * i);
        int y = sy + (int)lroundf(cosf(a) * i);
        g.fillCircle(x, y, i < 4 ? 4 : (i < 10 ? 3 : 2), c);
    }
}

static void zzz(TFT_eSPI& g, int x, int y, uint32_t t, uint16_t bg) {
    const uint32_t ZP = 3600;
    for (int i = 0; i < 3; i++) {
        float zp = (float)((t + i * (ZP / 3)) % ZP) / ZP;
        float a  = zp < 0.15f ? zp / 0.15f : (1.0f - zp) / 0.85f;
        g.setTextColor(mix565(C_ZZZ, bg, (uint8_t)(a * 255.0f)));
        g.drawString(zp < 0.5f ? "z" : "Z", x + (int)(zp * 16.0f), y - (int)(zp * 30.0f),
                     zp < 0.25f ? 1 : (zp < 0.7f ? 2 : 4));
    }
}

// ---- awake -------------------------------------------------------------------------

static void drawAwake(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const bool idle = p.mood == CatMood::IDLE;
    const CatAct act = idle ? p.act : CatAct::SIT;
    const float ph = p.actPhase;
    const int f = p.facing >= 0 ? 1 : -1;
    const uint32_t t = p.nowMs;
    const bool cold = p.comfort == Comfort::COLD, hot = p.comfort == Comfort::HOT;
    const int ground = oy + FLOOR_IN + 4;

    // Height: hop / petting bounce / tiptoe stretch.
    int lift = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = t - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        lift = (int)lroundf(5.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        lift = (int)lroundf(14.0f * bump(ph));
    }
    const float st = act == CatAct::STRETCH ? bump(ph) : 0.0f;
    const bool walking = act == CatAct::WALK;
    const int step = walking ? (int)((t / 160) % 2) : 0;
    const int sway = walking ? (step ? 2 : -2) : 0;             // waddle
    const int cx = ox + CAT_W / 2;
    const int feetY = ground - 2 - lift;
    const int tall = (int)lroundf(4.0f * st);
    const int bodyCy = feetY - 22 - tall;                       // body centre
    const int top = bodyCy - 26;                                // top of the head

    // Shadow.
    g.fillEllipse(cx, ground, 18 - lift / 2, 3, C_SHADOW);

    // Feet (alternate while waddling).
    g.fillEllipse(cx - 7, feetY + (walking && step ? -2 : 0), 6, 3, C_BEAK);
    g.fillEllipse(cx + 7, feetY + (walking && !step ? -2 : 0), 6, 3, C_BEAK);

    // Flippers behind/at the sides of the body.
    float aL = 0.35f, aR = 0.35f;                                 // resting, slightly out
    if (st > 0) { aL = aR = 0.35f + 2.4f * st; }                  // up over the head
    if (act == CatAct::HOP) aL = aR = 0.35f + 1.0f * bump(ph);
    if (p.mood == CatMood::PETTED) aL = aR = 0.6f + 0.6f * fabsf(sinf((float)t / 90.0f));
    if (walking) { aL = 0.35f + (step ? 0.4f : 0.0f); aR = 0.35f + (step ? 0.0f : 0.4f); }
    if (hot && idle && act == CatAct::SIT) aR = 1.2f + 0.5f * sinf((float)t / 70.0f);   // fanning
    const bool preen = act == CatAct::GROOM;
    if (preen) { if (f > 0) aL = 1.3f; else aR = 1.3f; }
    flipper(g, cx - 15 + sway, bodyCy - 8, -1, aL, C_BACK);
    flipper(g, cx + 15 + sway, bodyCy - 8, 1, aR, C_BACK);

    // Body: dark back, white belly.
    g.fillEllipse(cx + sway, bodyCy, 18, 24 + tall / 2, C_BACK);
    g.fillEllipse(cx + sway + f * 2, bodyCy + 4, 13, 18 + tall / 2, C_BELLY);
    g.fillEllipse(cx + sway + f * 2, bodyCy + 14, 9, 5, C_BELLY_D);
    // Head (round, on top of the body) with a white face mask.
    const int hx = cx + sway, hy = top + 13;
    g.fillCircle(hx, hy, 14, C_BACK);
    g.fillCircle(hx - 2, hy - 6, 3, C_BACK_L);                   // shine
    int lookOff = 0;
    if (act == CatAct::LOOK) lookOff = (int)lroundf(4.0f * p.look * hold(ph));
    const int fx = hx + f * 3 + lookOff + (preen ? -f * 4 : 0);
    g.fillEllipse(fx - 5, hy + 2, 6, 7, C_BELLY);
    g.fillEllipse(fx + 5, hy + 2, 6, 7, C_BELLY);
    g.fillEllipse(fx, hy + 6, 8, 5, C_BELLY);

    // Eyes.
    const int ey = hy + 1;
    const bool yawn = act == CatAct::YAWN;
    for (int s = -1; s <= 1; s += 2) {
        int ex = fx + s * 5;
        if (p.mood == CatMood::PETTED) {
            g.drawLine(ex - 2, ey + 1, ex, ey - 1, C_EYE);
            g.drawLine(ex, ey - 1, ex + 2, ey + 1, C_EYE);
        } else if (p.blink || yawn || preen) {
            g.drawFastHLine(ex - 2, ey, 5, C_EYE);
        } else if (p.mood == CatMood::WOKEN) {
            g.fillRect(ex - 2, ey, 5, 2, C_EYE);
        } else {
            g.fillCircle(ex, ey, 2, C_EYE);
            g.drawPixel(ex + 1, ey - 1, C_BELLY);
        }
    }
    g.fillCircle(fx - 9, hy + 6, 2, C_CHEEK);
    g.fillCircle(fx + 9, hy + 6, 2, C_CHEEK);

    // Beak (toward the facing side; wide open when yawning).
    {
        int bx = fx + f * 2, by = hy + 6;
        if (preen) { bx = hx - f * 10; by = hy + 10; }
        if (yawn) {
            int o = 2 + (int)lroundf(4.0f * bump(ph));
            g.fillTriangle(bx - 4, by - 1, bx + 4, by - 1, bx + f * 8, by - 2 - o, C_BEAK);
            g.fillTriangle(bx - 4, by + 1, bx + 4, by + 1, bx + f * 7, by + 2 + o, C_BEAK_D);
        } else {
            g.fillTriangle(bx - 4, by - 1, bx + 4, by - 1, bx + f * 8, by + 1, C_BEAK);
            g.fillTriangle(bx - 3, by + 1, bx + 3, by + 1, bx + f * 7, by + 2, C_BEAK_D);
        }
    }

    // Scarf in the comfort colour, end fluttering.
    {
        const uint16_t sc = p.body, scD = mix565(p.body, 0x0000, 190);
        g.fillRoundRect(hx - 14, hy + 11, 28, 6, 3, sc);
        int flap = walking || p.mood == CatMood::PETTED ? (int)((t / 120) % 3) - 1 : 0;
        g.fillRoundRect(hx - f * 10 - 3 + flap, hy + 14, 6, 13, 2, scD);
        g.drawFastHLine(hx - f * 10 - 3 + flap, hy + 24, 6, sc);
    }

    // Letter held against the belly.
    if (p.envelope) {
        int ex = cx + sway - 9, eyy = bodyCy + 2 + (int)((t / 300) % 2);
        g.fillRect(ex, eyy, 18, 12, C_PAPER);
        g.drawRect(ex, eyy, 18, 12, C_BELLY_D);
        g.drawLine(ex, eyy, ex + 9, eyy + 6, C_BACK_L);
        g.drawLine(ex + 17, eyy, ex + 9, eyy + 6, C_BACK_L);
        g.fillCircle(ex + 9, eyy + 6, 1, C_HEART);
    }

    // Comfort / humidity extras.
    if (cold) {                                             // happy sparkles
        for (int i = 0; i < 3; i++) {
            if (((t / 300) + i) % 3 == 0) continue;
            sparkle(g, cx - 28 + i * 26, top + 2 + (i % 2) * 10, 2, C_SPARKLE);
        }
    } else if (hot) {
        for (int i = 0; i < 2; i++) {
            int k = (int)(((t + i * 400) / 110) % 9);
            g.fillCircle(hx - 12 + i * 24, top + 6 + k, 1, C_DROP);
            g.drawPixel(hx - 12 + i * 24, top + 4 + k, C_DROP);
        }
    }
    if (p.humid == HumidMood::HUMID) drop(g, hx + f * 18, top + (int)((t / 150) % 6));
    if (p.mood == CatMood::PETTED) heart(g, hx + f * 18, oy + 12 - (int)(petPhase * 12.0f), 3, C_HEART);
}

// ---- asleep: sitting in a night cap ----------------------------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint32_t t = p.nowMs;
    const int cx = ox + CAT_W / 2, ground = oy + FLOOR_IN + 4;
    const uint16_t back = mix565(C_BACK, 0x0000, 220), belly = mix565(C_BELLY, 0x0000, 170);

    g.fillEllipse(cx, ground, 22, 3, C_SHADOW);
    float br = sinf((float)(t % 3600) * (2.0f * PI_F / 3600.0f));
    const int ry = 17 + (int)lroundf(1.2f * br);
    // Sitting: wide low body, flippers tucked, feet poking out.
    g.fillEllipse(cx - 8, ground - 1, 6, 3, mix565(C_BEAK, 0x0000, 170));
    g.fillEllipse(cx + 8, ground - 1, 6, 3, mix565(C_BEAK, 0x0000, 170));
    g.fillEllipse(cx, ground - 4 - ry, 21, ry, back);
    g.fillEllipse(cx, ground - 2 - ry, 15, ry - 4, belly);
    // Head drooped forward.
    const int hx = cx + 2, hy = ground - 2 * ry - 8;
    g.fillCircle(hx, hy, 13, back);
    g.fillEllipse(hx + 1, hy + 4, 10, 7, belly);
    for (int s = -1; s <= 1; s += 2)                               // closed eyes
        for (int k = 0; k < 2; k++) {
            g.drawLine(hx + s * 5 - 2, hy + 1 + k, hx + s * 5, hy + 3 + k, C_EYE);
            g.drawLine(hx + s * 5, hy + 3 + k, hx + s * 5 + 2, hy + 1 + k, C_EYE);
        }
    g.fillTriangle(hx - 3, hy + 6, hx + 3, hy + 6, hx, hy + 11, mix565(C_BEAK, 0x0000, 190));
    g.fillRoundRect(hx - 13, hy + 10, 26, 5, 2, mix565(p.body, 0x0000, 170));   // scarf
    // Night cap flopping to the side.
    g.fillTriangle(hx - 12, hy - 7, hx + 12, hy - 7, hx + 4, hy - 24, C_CAP);
    g.fillTriangle(hx + 1, hy - 18, hx + 6, hy - 24, hx + 18, hy - 12, C_CAP);
    g.fillRoundRect(hx - 13, hy - 10, 26, 5, 2, C_CAP_B);
    g.fillCircle(hx + 19, hy - 11, 3, C_CAP_B);
    zzz(g, hx - 30, hy - 8, t, rgb565(0x0e, 0x18, 0x3a));
}

// ---- entry point -----------------------------------------------------------------------

void catDraw(const CatPose& p, int x, int y) {
    TFT_eSPI& g = haveSprite ? (TFT_eSPI&)spr : tft;
    int ox = haveSprite ? 0 : x;
    int oy = haveSprite ? 0 : y;
    if (haveSprite) drawSceneLower(spr, x, y);
    else            sceneRepaintRect(x, y, CAT_W, CAT_H);
    if (p.mood == CatMood::SLEEPING) drawAsleep(g, p, ox, oy);
    else                             drawAwake(g, p, ox, oy);
    if (haveSprite) spr.pushSprite(x, y);
}
