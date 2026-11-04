// firmware/src/ui/cat_draw.cpp -- cat mascot drawn with TFT_eSPI primitives
//
// Every frame is painted into one CAT_W x CAT_H 16-bit sprite whose
// background is C_BG, so pushSprite needs no transparency and never flickers.
// The shapes follow the "B - Gatito" mock-up: triangle ears, round body,
// dark eyes with highlights, pink nose, "w" mouth, whiskers; asleep it curls
// into a ball ("hecho bola") with closed-eye arcs and drifting "Zzz".
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_PINK   = rgb565(0xf7, 0xa1, 0xb5);
static constexpr uint16_t C_HEART  = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_DROP   = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_ZZZ    = rgb565(0x8f, 0xa3, 0xff);
static constexpr uint16_t C_WHITE  = 0xFFFF;
static constexpr uint16_t C_SHADOW = 0x0000;
static constexpr uint16_t C_EYE    = C_BG;

static const float PI_F = 3.14159265f;

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[cat] sprite alloc failed, drawing direct");
    return haveSprite;
}

// ---- small parts ---------------------------------------------------------

// Thick (2 px) line: one pass plus one shifted down a pixel.
static void line2(TFT_eSPI& g, int x0, int y0, int x1, int y1, uint16_t c) {
    g.drawLine(x0, y0, x1, y1, c);
    g.drawLine(x0, y0 + 1, x1, y1 + 1, c);
}

static void heart(TFT_eSPI& g, int x, int y, uint16_t c) {
    g.fillCircle(x - 2, y, 2, c);
    g.fillCircle(x + 2, y, 2, c);
    g.fillTriangle(x - 4, y + 1, x + 4, y + 1, x, y + 5, c);
}

static void sweatDrop(TFT_eSPI& g, int x, int y) {
    g.fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y - 3, C_DROP);
    g.fillCircle(x, y + 4, 3, C_DROP);
    g.drawPixel(x - 1, y + 3, C_WHITE);
}

// Eye. closed: line (blink / yawn / stretch); happy: "^"; pdx/pdy: pupil
// offset (looking around / walking direction).
static void eye(TFT_eSPI& g, int ex, int ey, uint16_t body, bool closed, bool happy,
                bool sleepy, int pdx, int pdy) {
    if (happy) {
        line2(g, ex - 5, ey + 2, ex, ey - 3, C_EYE);
        line2(g, ex, ey - 3, ex + 5, ey + 2, C_EYE);
        return;
    }
    if (closed) {
        g.drawFastHLine(ex - 5, ey, 11, C_EYE);
        g.drawFastHLine(ex - 5, ey + 1, 11, C_EYE);
        return;
    }
    g.fillCircle(ex + pdx, ey + pdy, 5, C_EYE);
    if (sleepy) {                               // half-lid at night
        g.fillRect(ex - 7, ey - 7, 15, 5, body);
        g.drawFastHLine(ex - 5, ey - 2, 11, C_EYE);
        g.fillCircle(ex + pdx + 2, ey + 1, 1, C_WHITE);
        return;
    }
    g.fillCircle(ex + pdx + 2, ey + pdy - 2, 1, C_WHITE);
    g.drawPixel(ex + pdx - 2, ey + pdy + 2, C_WHITE);
}

// 0 at the ends of an act, 1 in the middle (smooth in/out).
static float bump(float ph) { return sinf(ph * PI_F); }
// 0 -> 1 quickly, held, then back to 0 at the end.
static float hold(float ph) {
    if (ph < 0.15f) return ph / 0.15f;
    if (ph > 0.85f) return (1.0f - ph) / 0.15f;
    return 1.0f;
}

// ---- awake: IDLE / WOKEN / PETTED (+ idle acts) ---------------------------

static void drawAwake(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint16_t body  = p.body;
    const uint16_t light = mix565(body, C_WHITE, 170);
    const uint16_t dark  = darken565(body);
    const uint16_t blush = mix565(C_PINK, body, 140);
    const bool     idle  = p.mood == CatMood::IDLE;
    const CatAct   act   = idle ? p.act : CatAct::SIT;
    const float    ph    = p.actPhase;
    const int      face  = p.facing >= 0 ? 1 : -1;

    // Cold: shiver left/right by a pixel while sitting.
    int shiver = 0;
    if (idle && act == CatAct::SIT && p.comfort == Comfort::COLD)
        shiver = ((p.nowMs / 70) % 2) ? 1 : -1;
    const int cx = ox + CAT_W / 2 + shiver;

    // Vertical offset: petting hop, HOP act, walking bob, else breathing.
    int hop = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = p.nowMs - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        hop = (int)lroundf(6.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        hop = (int)lroundf(8.0f * bump(ph));
    }
    const int step = (act == CatAct::WALK) ? (int)((p.nowMs / 180) % 2) : 0;
    int breathe = sinf((float)(p.nowMs % 3200) * (2.0f * PI_F / 3200.0f)) > 0.5f ? 1 : 0;
    if (act == CatAct::WALK) breathe = step;
    const float st = (act == CatAct::STRETCH) ? bump(ph) : 0.0f;   // 0..1
    const int by = oy + 8 - hop + (hop ? 0 : breathe) + (int)(4 * st);  // ear tips

    // Ground shadow stays put and shrinks as the cat leaves the floor.
    g.fillEllipse(cx, oy + 83, 26 - hop + (int)(5 * st), 3, C_SHADOW);

    // Tail behind the body, on the side opposite to where the cat faces.
    {
        const int side = -face;
        float period = (act == CatAct::WALK) ? 700.0f : 2800.0f;
        float amp    = (act == CatAct::WALK || act == CatAct::HOP) ? 5.0f : 3.0f;
        int sway = (int)lroundf(amp * sinf(fmodf((float)p.nowMs, period) * (2.0f * PI_F / period)));
        float up = (act == CatAct::HOP || p.mood == CatMood::PETTED) ? 8.0f : 0.0f;
        float x0 = cx + side * 22, y0 = by + 66;
        float xc = cx + side * 44, yc = by + 66;
        float x1 = cx + side * 38 + sway, y1 = by + 38 - up;
        for (int i = 0; i <= 10; i++) {
            float t = i / 10.0f, u = 1 - t;
            int x = (int)(u * u * x0 + 2 * u * t * xc + t * t * x1);
            int y = (int)(u * u * y0 + 2 * u * t * yc + t * t * y1);
            g.fillCircle(x, y, 3, body);
        }
    }

    // Ears (occasional twitch; flatter while stretching).
    int twL = (p.nowMs % 5200) < 160 ? 3 : 0;
    int twR = ((p.nowMs + 2600) % 7100) < 160 ? 3 : 0;
    int ef = (int)(4 * st);
    g.fillTriangle(cx - 28 - ef, by + 24, cx - 24 - twL - ef, by + (twL ? 2 : 0) + ef, cx - 8, by + 17, body);
    g.fillTriangle(cx + 28 + ef, by + 24, cx + 24 + twR + ef, by + (twR ? 2 : 0) + ef, cx + 8, by + 17, body);
    g.fillTriangle(cx - 23 - ef, by + 20, cx - 22 - twL - ef, by + 6 + ef, cx - 13, by + 17, C_PINK);
    g.fillTriangle(cx + 23 + ef, by + 20, cx + 22 + twR + ef, by + 6 + ef, cx + 13, by + 17, C_PINK);

    // Head + body blob (wider and flatter while stretching), belly.
    const int rx = 31 + (int)(5 * st), ry = 27 - (int)(4 * st);
    g.fillEllipse(cx, by + 44, rx, ry, body);
    g.fillEllipse(cx, by + 62 - (int)(2 * st), 14, 8, light);

    // Front paws: alternate while walking, pushed out while stretching,
    // one raised to the mouth while grooming.
    int lpY = by + 70, rpY = by + 70;
    int lpX = cx - 12 - (int)(8 * st), rpX = cx + 12 + (int)(8 * st);
    if (act == CatAct::WALK) { if (step) lpY -= 3; else rpY -= 3; }
    const bool groom = act == CatAct::GROOM;
    if (!groom || face > 0) g.fillEllipse(lpX, lpY, 7, 4, light);
    if (!groom || face < 0) g.fillEllipse(rpX, rpY, 7, 4, light);

    // Face. Pupils follow the walking direction or the LOOK target.
    int pdx = 0, pdy = 0;
    if (act == CatAct::WALK) pdx = 2 * face;
    if (act == CatAct::LOOK) { pdx = (int)lroundf(3.0f * p.look * hold(ph)); pdy = -1; }
    const bool yawnOpen = act == CatAct::YAWN && ph > 0.15f && ph < 0.9f;
    const bool closed = p.blink || yawnOpen || st > 0.3f;
    const bool happy  = p.mood == CatMood::PETTED || groom;
    const bool sleepy = p.mood == CatMood::WOKEN;
    eye(g, cx - 11, by + 39, body, closed, happy, sleepy, pdx, pdy);
    eye(g, cx + 11, by + 39, body, closed, happy, sleepy, pdx, pdy);
    g.fillCircle(cx - 20, by + 49, 3, blush);
    g.fillCircle(cx + 20, by + 49, 3, blush);
    g.fillTriangle(cx - 3, by + 46, cx + 3, by + 46, cx, by + 49, C_PINK);

    const bool pant = p.humid == HumidMood::DRY || p.comfort == Comfort::HOT;
    if (act == CatAct::YAWN) {                  // big yawn, opening and closing
        float o = bump(ph);
        int myr = 1 + (int)lroundf(6.0f * o), mxr = 2 + (int)lroundf(3.0f * o);
        g.fillEllipse(cx, by + 52 + myr / 2, mxr, myr, C_EYE);
        if (myr > 3) g.fillEllipse(cx, by + 52 + myr, mxr - 1, 2, C_PINK);
    } else if (groom) {                         // licking: tongue out
        g.fillEllipse(cx, by + 53, 2, 2, C_EYE);
        g.fillEllipse(cx + face, by + 55, 2, 2, C_PINK);
    } else if (pant) {                          // panting: open mouth + bobbing tongue
        int tb = (int)((p.nowMs / 220) % 2);
        g.fillEllipse(cx, by + 53, 3, 3, C_EYE);
        g.fillEllipse(cx, by + 55 + tb, 2, 1 + tb, C_PINK);
    } else {                                    // "w" mouth
        g.drawLine(cx, by + 49, cx - 2, by + 52, C_EYE);
        g.drawLine(cx - 2, by + 52, cx - 5, by + 50, C_EYE);
        g.drawLine(cx, by + 49, cx + 2, by + 52, C_EYE);
        g.drawLine(cx + 2, by + 52, cx + 5, by + 50, C_EYE);
    }

    // Whiskers (darkened body: visible on the fur and on the background).
    g.drawLine(cx - 15, by + 48, cx - 34, by + 45, dark);
    g.drawLine(cx - 15, by + 51, cx - 34, by + 53, dark);
    g.drawLine(cx + 15, by + 48, cx + 34, by + 45, dark);
    g.drawLine(cx + 15, by + 51, cx + 34, by + 53, dark);

    // Grooming paw drawn last, over the face, bobbing at the mouth.
    if (groom) {
        int lick = (int)lroundf(3.0f * fabsf(sinf(ph * PI_F * 7.0f)));
        int px = cx + face * 9, py = by + 58 - lick;
        g.fillEllipse(px, py, 6, 5, light);
        g.drawFastHLine(px - 2, py - 2, 5, dark);
    }

    if (p.humid == HumidMood::HUMID) {          // sweat drop sliding down
        sweatDrop(g, cx + 24, by + 24 + (int)((p.nowMs / 150) % 6));
    }
    if (p.mood == CatMood::PETTED) {            // heart floating up
        heart(g, cx + 38, oy + 22 - (int)(petPhase * 14.0f), C_HEART);
    }
    if (shiver && (p.nowMs / 700) % 4 == 0) {   // little cold puffs
        g.drawCircle(cx + 36, by + 52, 2, C_DROP);
        g.drawCircle(cx + 41, by + 49, 1, C_DROP);
    }
}

// ---- asleep: curled ball, closed eyes, Zzz ---------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint16_t body = darken565(p.body);
    const uint16_t tail = mix565(p.body, 0x0000, 180);   // lighter than the ball
    const uint16_t pink = darken565(C_PINK);
    const int cx = ox + CAT_W / 2;
    const int cy = oy + 64;
    // Slow breathing: the ball swells by 1 px.
    int ry = sinf((float)(p.nowMs % 4000) * (2.0f * PI_F / 4000.0f)) > 0 ? 14 : 13;

    g.fillEllipse(cx + 2, oy + 80, 34, 3, C_SHADOW);
    g.fillEllipse(cx + 4, cy, 34, ry, body);

    // Head tucked on the left, little ears.
    const int hx = cx - 18, hy = cy - 3;
    g.fillTriangle(hx - 13, hy - 6, hx - 11, hy - 20, hx - 2, hy - 13, body);
    g.fillTriangle(hx + 2, hy - 14, hx + 10, hy - 19, hx + 12, hy - 7, body);
    g.fillTriangle(hx - 10, hy - 9, hx - 9, hy - 16, hx - 5, hy - 12, pink);
    g.fillTriangle(hx + 4, hy - 13, hx + 9, hy - 16, hx + 9, hy - 9, pink);
    g.fillCircle(hx, hy, 15, body);

    // Tail wrapped along the front, tip under the chin.
    for (int i = 0; i <= 12; i++) {
        float a = (i / 12.0f) * 0.8f * PI_F;
        int x = (int)(cx + 11 + 26.0f * cosf(a));
        int y = (int)(cy + 2 + 9.0f * sinf(a));
        g.fillCircle(x, y, 3, tail);
    }

    // Closed-eye arcs and tiny nose.
    for (int s = -1; s <= 1; s += 2) {
        int ex = hx + s * 6, ey = hy - 2;
        g.drawLine(ex - 3, ey, ex - 1, ey + 2, C_EYE);
        g.drawLine(ex - 1, ey + 2, ex + 1, ey + 2, C_EYE);
        g.drawLine(ex + 1, ey + 2, ex + 3, ey, C_EYE);
        g.drawLine(ex - 3, ey + 1, ex - 1, ey + 3, C_EYE);
        g.drawLine(ex + 1, ey + 3, ex + 3, ey + 1, C_EYE);
    }
    g.fillTriangle(hx - 2, hy + 4, hx + 2, hy + 4, hx, hy + 6, pink);

    // "Zzz": three letters on a 3 s loop, staggered, rising and fading.
    const uint32_t ZP = 3000;
    for (int i = 0; i < 3; i++) {
        float ph = (float)((p.nowMs + i * (ZP / 3)) % ZP) / ZP;   // 0..1
        float a  = ph < 0.15f ? ph / 0.15f : (1.0f - ph) / 0.85f;
        int zx = cx + 10 + (int)(ph * 18.0f + 2.0f * sinf(ph * 2.0f * PI_F * 2.0f));
        int zy = oy + 40 - (int)(ph * 36.0f);
        g.setTextColor(mix565(C_ZZZ, C_BG, (uint8_t)(a * 255.0f)));
        g.drawString(ph < 0.66f ? "z" : "Z", zx, zy, ph < 0.33f ? 1 : 2);
    }
}

// ---- entry point -----------------------------------------------------------

void catDraw(const CatPose& p, int x, int y) {
    TFT_eSPI& g = haveSprite ? (TFT_eSPI&)spr : tft;
    int ox = haveSprite ? 0 : x;
    int oy = haveSprite ? 0 : y;
    g.fillRect(ox, oy, CAT_W, CAT_H, C_BG);
    if (p.mood == CatMood::SLEEPING) drawAsleep(g, p, ox, oy);
    else                             drawAwake(g, p, ox, oy);
    if (haveSprite) spr.pushSprite(x, y);
}
