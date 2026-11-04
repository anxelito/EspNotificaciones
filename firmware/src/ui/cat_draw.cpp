// firmware/src/ui/cat_draw.cpp -- the mascot: a goldfish in its tank
//
// (File and type names still say "cat": the mascot logic -- moods, idle acts,
// pose -- is shared by every design; only the drawing changes.) Each frame is
// painted into one CAT_W x CAT_H sprite whose background is the tank, so the
// fish never flickers or shows a box.
//
// Idle acts: WALK = swims across, LOOK = eye glances aside, GROOM = blows a
// string of bubbles, YAWN = mouth wide "O", STRETCH = fins spread wide,
// HOP = darts up and back. Moods: PETTED = happy eyes, wiggle, hearts;
// WOKEN = sleepy eye; SLEEPING = rests on the sand, dimmed, eye closed.
// Comfort: the dorsal fin stripe shows the room colour; cold = shiver and
// ice crystals, hot = flushed cheeks and fast fanning.
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"
#include "room_scene.h"
#include "screen_room.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_FISH    = rgb565(0xff, 0x8c, 0x3a);
static constexpr uint16_t C_FISH_D  = rgb565(0xe0, 0x62, 0x24);
static constexpr uint16_t C_FISH_L  = rgb565(0xff, 0xc8, 0x8a);
static constexpr uint16_t C_FIN     = rgb565(0xff, 0xa8, 0x5c);
static constexpr uint16_t C_EYEW    = rgb565(0xff, 0xff, 0xff);
static constexpr uint16_t C_EYE     = rgb565(0x14, 0x16, 0x1e);
static constexpr uint16_t C_MOUTH   = rgb565(0x7a, 0x2a, 0x1e);
static constexpr uint16_t C_CHEEK   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_HEART   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_BUBBLE  = rgb565(0xd8, 0xf2, 0xff);
static constexpr uint16_t C_ICE     = rgb565(0xe6, 0xf6, 0xff);
static constexpr uint16_t C_ZZZ     = rgb565(0xe8, 0xec, 0xff);
static constexpr uint16_t C_PAPER   = rgb565(0xf4, 0xf1, 0xe8);
static constexpr uint16_t C_SHADOW  = rgb565(0xc4, 0xaa, 0x76);

static const float PI_F = 3.14159265f;
static const int FLOOR_IN = ROOM_FLOOR_Y - ROOM_CAT_Y;   // sand line inside the sprite

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[fish] sprite alloc failed, drawing direct");
    return haveSprite;
}

// ---- small parts ---------------------------------------------------------------

static void heart(TFT_eSPI& g, int x, int y, int s, uint16_t c) {
    g.fillCircle(x - s, y, s, c);
    g.fillCircle(x + s, y, s, c);
    g.fillTriangle(x - 2 * s, y + 1, x + 2 * s, y + 1, x, y + 2 * s + 1, c);
}

static void bubble(TFT_eSPI& g, int x, int y, int r) {
    g.drawCircle(x, y, r, C_BUBBLE);
    if (r > 1) g.drawPixel(x - 1, y - 1, C_BUBBLE);
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

// A goldfish facing f (+1 right), body centre (x,y). sw = tail swing,
// spread = fin spread 0..1, dimmed for sleep.
static void fishBody(TFT_eSPI& g, int x, int y, int f, int sw, float spread, uint16_t body,
                     uint16_t dark, uint16_t light, uint16_t fin, uint16_t stripe) {
    // Tail: two lobes behind, swinging.
    int tx = x - f * 20;
    g.fillTriangle(tx + f * 4, y, tx - f * 14, y - 12 + sw, tx - f * 9, y + sw / 2, fin);
    g.fillTriangle(tx + f * 4, y, tx - f * 14, y + 12 + sw, tx - f * 9, y + sw / 2, fin);
    g.drawLine(tx + f * 2, y, tx - f * 11, y - 8 + sw, dark);
    g.drawLine(tx + f * 2, y, tx - f * 11, y + 8 + sw, dark);
    // Dorsal fin with the comfort stripe, belly fin.
    int sp = (int)lroundf(5.0f * spread);
    g.fillTriangle(x - f * 8, y - 11, x + f * 6, y - 11, x - f * 4, y - 20 - sp, fin);
    g.drawLine(x - f * 6, y - 13, x - f * 3, y - 18 - sp, stripe);
    g.drawLine(x - f * 5, y - 13, x - f * 2, y - 18 - sp, stripe);
    g.fillTriangle(x - f * 6, y + 10, x + f * 4, y + 10, x - f * 6, y + 17 + sp, fin);
    // Body.
    g.fillEllipse(x, y, 21, 13, body);
    g.fillEllipse(x + f * 3, y + 5, 14, 6, light);
    // Scales: a few arcs.
    for (int i = 0; i < 3; i++) {
        int sx = x - f * (6 - i * 6), sy = y - 4 + (i % 2) * 3;
        g.drawLine(sx, sy - 2, sx + f * 2, sy, dark);
        g.drawLine(sx + f * 2, sy, sx, sy + 2, dark);
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
    const bool swim = act == CatAct::WALK;

    int shiver = (idle && act == CatAct::SIT && cold) ? (((t / 70) % 2) ? 1 : -1) : 0;
    const int cx = ox + CAT_W / 2 + 4 * f + shiver;

    // Height: gentle bob, dart up, petting wiggle.
    int lift = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = t - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        lift = (int)lroundf(6.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        lift = (int)lroundf(18.0f * bump(ph));
    }
    const int bob = (int)lroundf(4.0f * sinf((float)(t % 2400) * (2.0f * PI_F / 2400.0f)));
    const int cy = oy + 46 + bob - lift;

    // Shadow on the sand.
    g.fillEllipse(cx, oy + FLOOR_IN + 6, 18 - (bob + lift) / 3, 2, C_SHADOW);

    // Tail swing: lazy, fast while swimming / hot / petted.
    float rate = swim ? 70.0f : (hot || p.mood == CatMood::PETTED ? 60.0f : 200.0f);
    int sw = (int)lroundf(5.0f * sinf((float)t / rate));
    float spread = act == CatAct::STRETCH ? bump(ph) : 0.0f;
    fishBody(g, cx, cy, f, sw, spread, C_FISH, C_FISH_D, C_FISH_L, C_FIN, p.body);

    // Side fin paddling (wider when stretching).
    {
        int pad = (int)lroundf(3.0f * sinf((float)t / (hot ? 50.0f : 140.0f)));
        int fl = 7 + (int)lroundf(6.0f * spread);
        g.fillTriangle(cx + f * 2, cy + 2, cx - f * 4, cy + 2 + fl + pad, cx - f * (8 + (int)(4 * spread)), cy + 4, C_FIN);
    }

    // Eye.
    const int ex = cx + f * 11, ey = cy - 3;
    int lookOff = 0;
    if (act == CatAct::LOOK) lookOff = (int)lroundf(2.0f * p.look * hold(ph));
    const bool yawn = act == CatAct::YAWN;
    if (p.mood == CatMood::PETTED) {
        g.drawLine(ex - 3, ey + 1, ex, ey - 2, C_EYE);
        g.drawLine(ex, ey - 2, ex + 3, ey + 1, C_EYE);
        g.drawLine(ex - 3, ey + 2, ex, ey - 1, C_EYE);
        g.drawLine(ex, ey - 1, ex + 3, ey + 2, C_EYE);
    } else {
        g.fillCircle(ex, ey, 5, C_EYEW);
        if (p.blink || yawn) {
            g.fillRect(ex - 4, ey, 9, 2, C_EYE);
        } else {
            g.fillCircle(ex + f * 1 + lookOff, ey, 3, C_EYE);
            g.drawPixel(ex + lookOff, ey - 1, C_EYEW);
            if (p.mood == CatMood::WOKEN) g.fillRect(ex - 5, ey - 5, 11, 4, C_FISH);
        }
    }
    if (hot) g.fillCircle(cx + f * 8, cy + 5, 3, mix565(C_CHEEK, C_FISH, 200));
    else     g.fillCircle(cx + f * 8, cy + 5, 2, mix565(C_CHEEK, C_FISH, 120));

    // Mouth: "O" when yawning / blowing bubbles, letter, small smile.
    const int mx = cx + f * 20, my = cy + 3;
    const bool blow = act == CatAct::GROOM;
    if (yawn) {
        int r = 2 + (int)lroundf(3.0f * bump(ph));
        g.fillCircle(mx, my, r, C_MOUTH);
    } else if (blow) {
        g.fillCircle(mx, my, 2, C_MOUTH);
        for (int i = 0; i < 4; i++) {                      // bubbles drifting up
            float b = fmodf(ph * 3.0f + i * 0.25f, 1.0f);
            bubble(g, mx + f * (3 + (int)(6 * b)), my - 4 - (int)(b * 40), 1 + (i % 3));
        }
    } else if (p.envelope) {
        int ly = my - 5 + (int)((t / 300) % 2);
        int lx = mx + (f > 0 ? -2 : -14);
        g.fillRect(lx, ly, 16, 11, C_PAPER);
        g.drawRect(lx, ly, 16, 11, C_FISH_D);
        g.drawLine(lx, ly, lx + 8, ly + 6, C_FISH_D);
        g.drawLine(lx + 15, ly, lx + 8, ly + 6, C_FISH_D);
        g.fillCircle(lx + 8, ly + 6, 1, C_HEART);
    } else {
        g.drawLine(mx - f * 3, my, mx, my + 1, C_MOUTH);
        g.drawLine(mx, my + 1, mx + f * 1, my - 1, C_MOUTH);
    }
    // Idle breathing bubble every few seconds.
    if (!blow && !yawn && (t / 3000) % 2 == 0) {
        float b = (float)(t % 3000) / 3000.0f;
        bubble(g, mx + f * 3, my - 4 - (int)(b * 34), 1 + (int)(b * 2));
    }

    // Cold: ice crystals around.
    if (cold) {
        for (int i = 0; i < 3; i++) {
            if (((t / 350) + i) % 3 == 0) continue;
            int sx = cx - 26 + i * 26, sy = cy - 24 + (i % 2) * 6;
            g.drawFastHLine(sx - 2, sy, 5, C_ICE);
            g.drawFastVLine(sx, sy - 2, 5, C_ICE);
            g.drawPixel(sx - 1, sy - 1, C_ICE); g.drawPixel(sx + 1, sy + 1, C_ICE);
        }
    }
    if (p.mood == CatMood::PETTED) heart(g, cx + f * 10, oy + 14 - (int)(petPhase * 12.0f), 3, C_HEART);
}

// ---- asleep: resting on the sand ----------------------------------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint32_t t = p.nowMs;
    const int cx = ox + CAT_W / 2;
    const int cy = oy + FLOOR_IN - 6 + (int)lroundf(1.0f * sinf((float)(t % 3600) * (2.0f * PI_F / 3600.0f)));
    const uint16_t body = mix565(C_FISH, 0x0000, 150), dark = mix565(C_FISH_D, 0x0000, 150);
    const uint16_t light = mix565(C_FISH_L, 0x0000, 150), fin = mix565(C_FIN, 0x0000, 150);

    g.fillEllipse(cx, oy + FLOOR_IN + 5, 22, 2, C_SHADOW);
    int sw = (int)lroundf(2.0f * sinf((float)t / 600.0f));
    fishBody(g, cx, cy, 1, sw, 0.0f, body, dark, light, fin, mix565(p.body, 0x0000, 150));
    // Closed eye.
    const int ex = cx + 11, ey = cy - 3;
    for (int k = 0; k < 2; k++) {
        g.drawLine(ex - 3, ey + k, ex - 1, ey + 2 + k, C_EYE);
        g.drawLine(ex - 1, ey + 2 + k, ex + 3, ey + k, C_EYE);
    }
    g.drawLine(cx + 17, cy + 4, cx + 20, cy + 4, C_MOUTH);
    // Slow sleepy bubbles + Zzz.
    float b = (float)(t % 4000) / 4000.0f;
    bubble(g, cx + 22, cy - 2 - (int)(b * 24), 1 + (int)(b * 2));
    zzz(g, cx - 10, cy - 20, t, rgb565(0x08, 0x1a, 0x30));
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
