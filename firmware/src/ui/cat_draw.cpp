// firmware/src/ui/cat_draw.cpp -- the mascot: a frog on its lily pad
//
// (File and type names still say "cat": the mascot logic -- moods, idle acts,
// pose -- is shared by every design; only the drawing changes.) Each frame is
// painted into one CAT_W x CAT_H sprite whose background is the pond, so the
// frog never flickers or shows a box. Its lily pad travels with it.
//
// Idle acts: WALK = paddles its pad along with little hops, LOOK = eyes
// glance aside, GROOM = catches a fly with its tongue, YAWN = croaks (the
// throat sac swells), STRETCH = stretches its back legs, HOP = big jump.
// Moods: PETTED = happy eyes, hearts; WOKEN = sleepy lids; SLEEPING = dozes
// on the pad under a leaf blanket. Comfort: the bow tie shows the room
// colour; cold = shiver + frost, hot = sweat + tongue out; humid = happy
// smile with a drop (frogs love it).
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"
#include "room_scene.h"
#include "screen_room.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_SKIN    = rgb565(0x6e, 0xc8, 0x4e);
static constexpr uint16_t C_SKIN_D  = rgb565(0x4c, 0xa0, 0x38);
static constexpr uint16_t C_SKIN_L  = rgb565(0xd6, 0xf0, 0xa8);
static constexpr uint16_t C_SPOT    = rgb565(0x3e, 0x88, 0x30);
static constexpr uint16_t C_EYEW    = rgb565(0xff, 0xff, 0xf4);
static constexpr uint16_t C_EYE     = rgb565(0x14, 0x16, 0x1e);
static constexpr uint16_t C_MOUTH   = rgb565(0x2a, 0x4a, 0x22);
static constexpr uint16_t C_TONGUE  = rgb565(0xff, 0x7a, 0x9a);
static constexpr uint16_t C_SAC     = rgb565(0xee, 0xf6, 0xc8);
static constexpr uint16_t C_PAD     = rgb565(0x4c, 0xb8, 0x64);
static constexpr uint16_t C_PAD_D   = rgb565(0x38, 0x96, 0x4e);
static constexpr uint16_t C_WATER   = rgb565(0x3a, 0x9c, 0xc6);
static constexpr uint16_t C_RIPPLE  = rgb565(0x8a, 0xd0, 0xec);
static constexpr uint16_t C_HEART   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_DROP    = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_FROST   = rgb565(0xe6, 0xf6, 0xff);
static constexpr uint16_t C_ZZZ     = rgb565(0xe8, 0xec, 0xff);
static constexpr uint16_t C_PAPER   = rgb565(0xf4, 0xf1, 0xe8);
static constexpr uint16_t C_FLY     = rgb565(0x22, 0x22, 0x2a);
static constexpr uint16_t C_FLYW    = rgb565(0xd8, 0xe8, 0xf4);
static constexpr uint16_t C_LEAF    = rgb565(0x8a, 0xc8, 0x4a);

static const float PI_F = 3.14159265f;
static const int FLOOR_IN = ROOM_FLOOR_Y - ROOM_CAT_Y;   // water line inside the sprite

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[frog] sprite alloc failed, drawing direct");
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

static float bump(float ph) { return sinf(ph * PI_F); }
static float hold(float ph) {
    if (ph < 0.15f) return ph / 0.15f;
    if (ph > 0.85f) return (1.0f - ph) / 0.15f;
    return 1.0f;
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

// The pad the frog sits on, with a ripple ring around it.
static void pad(TFT_eSPI& g, int cx, int y, uint32_t t, bool moving, uint16_t dim) {
    int r = 30 + (int)((t / 120) % 6);
    g.drawEllipse(cx, y + 1, r, 4, mix565(C_RIPPLE, C_WATER, moving ? 200 : 110));
    g.fillEllipse(cx, y, 28, 6, mix565(C_PAD_D, 0x0000, dim));
    g.fillEllipse(cx, y - 1, 26, 5, mix565(C_PAD, 0x0000, dim));
    g.fillTriangle(cx, y - 1, cx + 27, y - 4, cx + 27, y + 2, C_WATER);   // notch
    g.drawLine(cx, y - 1, cx - 18, y - 4, mix565(C_PAD_D, 0x0000, dim));
    g.drawLine(cx, y - 1, cx - 16, y + 2, mix565(C_PAD_D, 0x0000, dim));
}

// ---- awake -------------------------------------------------------------------------

static void drawAwake(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const bool idle = p.mood == CatMood::IDLE;
    const CatAct act = idle ? p.act : CatAct::SIT;
    const float ph = p.actPhase;
    const int f = p.facing >= 0 ? 1 : -1;
    const uint32_t t = p.nowMs;
    const bool cold = p.comfort == Comfort::COLD, hot = p.comfort == Comfort::HOT;
    const bool walking = act == CatAct::WALK;

    int shiver = (idle && act == CatAct::SIT && cold) ? (((t / 70) % 2) ? 1 : -1) : 0;
    const int cx = ox + CAT_W / 2;
    const int water = oy + FLOOR_IN + 2;

    pad(g, cx, water, t, walking, 255);

    // Height above the pad.
    int lift = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = t - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        lift = (int)lroundf(6.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        lift = (int)lroundf(26.0f * bump(ph));
    } else if (walking) {
        float hp = (float)(t % 500) / 500.0f;
        lift = (int)lroundf(5.0f * bump(hp));
    }
    const float st = act == CatAct::STRETCH ? bump(ph) : 0.0f;
    const bool air = lift > 3;
    const int fx = cx + shiver;
    const int base = water - 4 - lift;                       // bottom of the frog

    // Back legs: folded at the sides, kicked out while jumping / stretching.
    for (int s = -1; s <= 1; s += 2) {
        if (air || st > 0.2f) {
            int reach = air ? 10 : (int)(14 * st);
            g.fillEllipse(fx + s * (18 + reach / 2), base - 2 + reach / 3, 8 + reach / 3, 4, C_SKIN_D);
            g.fillEllipse(fx + s * (24 + reach), base + reach / 3, 5, 2, C_SKIN_D);
        } else {
            g.fillEllipse(fx + s * 17, base - 6, 9, 7, C_SKIN_D);
            g.fillEllipse(fx + s * 21, base, 7, 2, C_SKIN_D);
        }
    }
    // Body.
    const int by = base - 14;
    g.fillEllipse(fx, by, 20, 14, C_SKIN);
    g.fillEllipse(fx, by + 5, 13, 8, C_SKIN_L);
    g.fillCircle(fx - 12, by - 6, 2, C_SPOT);
    g.fillCircle(fx + 13, by - 3, 2, C_SPOT);
    // Front feet.
    g.fillEllipse(fx - 9, base - 1, 4, 2, C_SKIN_D);
    g.fillEllipse(fx + 9, base - 1, 4, 2, C_SKIN_D);

    // Head: wide, with two bulging eyes on top.
    const int hy = by - 14;
    g.fillEllipse(fx, hy, 19, 11, C_SKIN);
    int lookOff = 0;
    if (act == CatAct::LOOK) lookOff = (int)lroundf(3.0f * p.look * hold(ph));
    if (walking) lookOff = 2 * f;
    for (int s = -1; s <= 1; s += 2) {
        int ex = fx + s * 11, ey = hy - 9;
        g.fillCircle(ex, ey, 7, C_SKIN);
        if (p.mood == CatMood::PETTED) {
            g.drawLine(ex - 3, ey + 1, ex, ey - 2, C_EYE);
            g.drawLine(ex, ey - 2, ex + 3, ey + 1, C_EYE);
            g.drawLine(ex - 3, ey + 2, ex, ey - 1, C_EYE);
            g.drawLine(ex, ey - 1, ex + 3, ey + 2, C_EYE);
        } else if (p.blink) {
            g.fillRect(ex - 4, ey, 9, 2, C_EYE);
        } else {
            g.fillCircle(ex, ey, 5, C_EYEW);
            g.fillCircle(ex + lookOff, ey + 1, 3, C_EYE);
            g.drawPixel(ex + lookOff + 1, ey, C_EYEW);
            if (p.mood == CatMood::WOKEN) g.fillRect(ex - 5, ey - 5, 11, 5, C_SKIN_D);   // heavy lids
        }
    }

    // Croak: throat sac swells under the chin.
    const bool croak = act == CatAct::YAWN;
    if (croak) {
        int r = 3 + (int)lroundf(7.0f * bump(fmodf(ph * 2.0f, 1.0f)));
        g.fillCircle(fx, hy + 9, r, C_SAC);
        g.drawCircle(fx, hy + 9, r, C_SKIN_D);
    }

    // Mouth: wide smile; tongue catches a fly when grooming; tongue out when hot.
    const int my = hy + 3;
    if (act == CatAct::GROOM) {
        float k = ph < 0.5f ? bump(ph * 2.0f) : 0.0f;          // first half: zap
        int tx = fx + f * (int)(6 + 34 * k), ty = my - (int)(20 * k);
        int flyX = fx + f * 40, flyY = my - 20;
        if (ph < 0.25f) {                                       // fly buzzing nearby
            g.fillCircle(flyX, flyY, 2, C_FLY);
            g.fillEllipse(flyX - 1, flyY - 3, 2, 1, C_FLYW);
            g.fillEllipse(flyX + 2, flyY - 3, 2, 1, C_FLYW);
        }
        if (k > 0.05f) {
            g.drawLine(fx + f * 4, my, tx, ty, C_TONGUE);
            g.drawLine(fx + f * 4, my + 1, tx, ty + 1, C_TONGUE);
            g.fillCircle(tx, ty, 2, C_TONGUE);
        }
        g.drawFastHLine(fx - 9, my, 18, C_MOUTH);
        if (ph > 0.5f) {                                        // happy chewing
            int c = (int)((t / 120) % 2);
            g.fillEllipse(fx + 12, my - 1 - c, 3, 2, C_SKIN_L);
        }
    } else {
        for (int k = 0; k < 2; k++) {
            g.drawLine(fx - 11, my - 2 + k, fx - 4, my + 1 + k, C_MOUTH);
            g.drawLine(fx - 4, my + 1 + k, fx + 4, my + 1 + k, C_MOUTH);
            g.drawLine(fx + 4, my + 1 + k, fx + 11, my - 2 + k, C_MOUTH);
        }
        if (hot) g.fillEllipse(fx + 3, my + 4 + (int)((t / 180) % 2), 3, 3, C_TONGUE);
    }
    g.fillCircle(fx - 14, hy + 2, 2, mix565(C_TONGUE, C_SKIN, 120));    // cheeks
    g.fillCircle(fx + 14, hy + 2, 2, mix565(C_TONGUE, C_SKIN, 120));

    // Bow tie in the comfort colour.
    {
        const int ty = hy + 11 + (croak ? 6 : 0);
        g.fillTriangle(fx - 8, ty - 4, fx - 8, ty + 4, fx, ty, p.body);
        g.fillTriangle(fx + 8, ty - 4, fx + 8, ty + 4, fx, ty, p.body);
        g.fillCircle(fx, ty, 2, mix565(p.body, 0x0000, 180));
    }

    // Letter held up in front.
    if (p.envelope) {
        int ex = fx - 9, eyy = by - 2 + (int)((t / 300) % 2);
        g.fillRect(ex, eyy, 18, 12, C_PAPER);
        g.drawRect(ex, eyy, 18, 12, C_SKIN_D);
        g.drawLine(ex, eyy, ex + 9, eyy + 6, C_SKIN_D);
        g.drawLine(ex + 17, eyy, ex + 9, eyy + 6, C_SKIN_D);
        g.fillCircle(ex + 9, eyy + 6, 1, C_HEART);
    }

    // Comfort / humidity extras.
    if (cold) {
        g.fillTriangle(fx - 17, hy - 5, fx - 11, hy - 8, fx - 15, hy, C_FROST);
        g.fillTriangle(fx + 17, hy - 5, fx + 11, hy - 8, fx + 15, hy, C_FROST);
    } else if (hot) {
        int k = (int)((t / 120) % 8);
        g.fillCircle(fx + 20, hy - 6 + k, 1, C_DROP);
        g.drawPixel(fx + 20, hy - 8 + k, C_DROP);
    }
    if (p.humid == HumidMood::HUMID) drop(g, fx - 22, hy - 16 + (int)((t / 150) % 6));
    if (p.mood == CatMood::PETTED) heart(g, fx + 22, oy + 12 - (int)(petPhase * 12.0f), 3, C_HEART);
}

// ---- asleep: under a leaf blanket ------------------------------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint32_t t = p.nowMs;
    const int cx = ox + CAT_W / 2, water = oy + FLOOR_IN + 2;
    const uint16_t skin = mix565(C_SKIN, 0x0000, 170), skinD = mix565(C_SKIN_D, 0x0000, 170);

    pad(g, cx, water, t, false, 160);
    float br = sinf((float)(t % 3600) * (2.0f * PI_F / 3600.0f));
    const int base = water - 4;
    // Flattened, legs tucked.
    g.fillEllipse(cx - 17, base - 4, 9, 5, skinD);
    g.fillEllipse(cx + 17, base - 4, 9, 5, skinD);
    g.fillEllipse(cx, base - 10, 21, 10 + (int)lroundf(br), skin);
    const int hy = base - 20;
    g.fillEllipse(cx, hy, 18, 9, skin);
    for (int s = -1; s <= 1; s += 2) {                      // closed eye bumps
        g.fillCircle(cx + s * 10, hy - 7, 6, skin);
        g.drawFastHLine(cx + s * 10 - 4, hy - 6, 9, C_EYE);
        g.drawFastHLine(cx + s * 10 - 3, hy - 5, 7, C_EYE);
    }
    g.drawFastHLine(cx - 8, hy + 3, 16, C_MOUTH);
    // Leaf blanket over the body.
    g.fillEllipse(cx + 2, base - 7, 22, 7, mix565(C_LEAF, 0x0000, 190));
    g.drawLine(cx - 18, base - 7, cx + 22, base - 7, mix565(C_PAD_D, 0x0000, 190));
    for (int i = -12; i <= 12; i += 8)
        g.drawLine(cx + i, base - 7, cx + i + 5, base - 12, mix565(C_PAD_D, 0x0000, 190));
    g.fillCircle(cx - 6, hy + 8, 2, mix565(p.body, 0x0000, 170));          // bow tie peeking out
    zzz(g, cx + 14, hy - 16, t, rgb565(0x0e, 0x18, 0x3a));
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
