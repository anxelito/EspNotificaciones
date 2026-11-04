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

void catDrawInit() {
    if (haveSprite) return;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[cat] sprite alloc failed, drawing direct");
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

// Open eye with two highlights; mood tweaks the lid.
static void eye(TFT_eSPI& g, const CatPose& p, int ex, int ey, uint16_t body) {
    if (p.mood == CatMood::PETTED) {          // happy "^"
        line2(g, ex - 5, ey + 2, ex, ey - 3, C_EYE);
        line2(g, ex, ey - 3, ex + 5, ey + 2, C_EYE);
        return;
    }
    if (p.blink) {                             // closed line
        g.drawFastHLine(ex - 5, ey, 11, C_EYE);
        g.drawFastHLine(ex - 5, ey + 1, 11, C_EYE);
        return;
    }
    g.fillCircle(ex, ey, 5, C_EYE);
    if (p.mood == CatMood::WOKEN) {            // sleepy half-lid at night
        g.fillRect(ex - 6, ey - 6, 13, 4, body);
        g.drawFastHLine(ex - 5, ey - 2, 11, C_EYE);
        g.fillCircle(ex + 2, ey + 1, 1, C_WHITE);
        return;
    }
    g.fillCircle(ex + 2, ey - 2, 1, C_WHITE);
    g.drawPixel(ex - 2, ey + 2, C_WHITE);
}

// ---- awake: IDLE / WOKEN / PETTED ------------------------------------------

static void drawAwake(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint16_t body  = p.body;
    const uint16_t light = mix565(body, C_WHITE, 170);
    const uint16_t dark  = darken565(body);
    const uint16_t blush = mix565(C_PINK, body, 140);
    const int cx = ox + CAT_W / 2;

    // Vertical offset: hop while petted, 1 px breathing otherwise.
    int hop = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = p.nowMs - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        hop = (int)lroundf(6.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    }
    int breathe = sinf((float)(p.nowMs % 3200) * (2.0f * PI_F / 3200.0f)) > 0.5f ? 1 : 0;
    const int by = oy + 8 - hop + (hop ? 0 : breathe);   // y of ear tips

    // Ground shadow stays put and shrinks as the cat leaves the floor.
    g.fillEllipse(cx, oy + 83, 26 - hop, 3, C_SHADOW);

    // Tail behind the body, tip swaying.
    {
        int sway = (int)lroundf(3.0f * sinf((float)(p.nowMs % 2800) * (2.0f * PI_F / 2800.0f)));
        float x0 = cx + 22, y0 = by + 66, xc = cx + 44, yc = by + 66, x1 = cx + 38 + sway, y1 = by + 38;
        for (int i = 0; i <= 10; i++) {
            float t = i / 10.0f, u = 1 - t;
            int x = (int)(u * u * x0 + 2 * u * t * xc + t * t * x1);
            int y = (int)(u * u * y0 + 2 * u * t * yc + t * t * y1);
            g.fillCircle(x, y, 3, body);
        }
    }

    // Ears (occasional twitch: tip flicks outward for ~160 ms).
    int twL = (p.nowMs % 5200) < 160 ? 3 : 0;
    int twR = ((p.nowMs + 2600) % 7100) < 160 ? 3 : 0;
    g.fillTriangle(cx - 28, by + 24, cx - 24 - twL, by + (twL ? 2 : 0), cx - 8, by + 17, body);
    g.fillTriangle(cx + 28, by + 24, cx + 24 + twR, by + (twR ? 2 : 0), cx + 8, by + 17, body);
    g.fillTriangle(cx - 23, by + 20, cx - 22 - twL, by + 6, cx - 13, by + 17, C_PINK);
    g.fillTriangle(cx + 23, by + 20, cx + 22 + twR, by + 6, cx + 13, by + 17, C_PINK);

    // Head + body as one round blob, lighter belly and paws.
    g.fillEllipse(cx, by + 44, 31, 27, body);
    g.fillEllipse(cx, by + 62, 14, 8, light);
    g.fillEllipse(cx - 12, by + 70, 7, 4, light);
    g.fillEllipse(cx + 12, by + 70, 7, 4, light);

    // Face.
    eye(g, p, cx - 11, by + 39, body);
    eye(g, p, cx + 11, by + 39, body);
    g.fillCircle(cx - 20, by + 49, 3, blush);
    g.fillCircle(cx + 20, by + 49, 3, blush);
    g.fillTriangle(cx - 3, by + 46, cx + 3, by + 46, cx, by + 49, C_PINK);
    if (p.humid == HumidMood::DRY) {           // panting: open mouth + tongue
        g.fillEllipse(cx, by + 53, 3, 3, C_EYE);
        g.fillEllipse(cx, by + 55, 2, 1, C_PINK);
    } else {                                   // "w" mouth
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

    if (p.humid == HumidMood::HUMID) {         // sweat drop sliding down
        sweatDrop(g, cx + 24, by + 24 + (int)((p.nowMs / 150) % 6));
    }
    if (p.mood == CatMood::PETTED) {           // heart floating up
        heart(g, cx + 38, oy + 22 - (int)(petPhase * 14.0f), C_HEART);
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
