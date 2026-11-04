// firmware/src/ui/cat_draw.cpp -- the mascot: a puppy in the garden
//
// (File and type names still say "cat": the mascot logic -- moods, idle acts,
// pose -- is shared by every design; only the drawing changes.) Each frame is
// painted into one CAT_W x CAT_H sprite whose background is the garden, so
// the puppy never flickers or shows a box.
//
// Idle acts: WALK = trots across, LOOK = turns its head and perks an ear,
// GROOM = scratches behind the ear, YAWN = big yawn, STRETCH = play bow,
// HOP = jumps. Moods: PETTED = happy eyes, tail going wild, hearts;
// WOKEN = sleepy eyes; SLEEPING = curled up on its cushion.
// Comfort: the collar shows the room colour; cold = scarf + shiver + breath
// puffs, hot = tongue out panting; humid = water drop.
#include "cat_draw.h"
#include <math.h>
#include "theme.h"
#include "../display.h"
#include "room_scene.h"
#include "screen_room.h"

static TFT_eSprite spr(&tft);
static bool        haveSprite = false;

static constexpr uint16_t C_FUR     = rgb565(0xe2, 0xaa, 0x6c);
static constexpr uint16_t C_FUR_D   = rgb565(0xa8, 0x68, 0x3a);
static constexpr uint16_t C_FUR_L   = rgb565(0xf8, 0xe0, 0xbc);
static constexpr uint16_t C_NOSE    = rgb565(0x2a, 0x1e, 0x1a);
static constexpr uint16_t C_EYE     = rgb565(0x1e, 0x16, 0x14);
static constexpr uint16_t C_TONGUE  = rgb565(0xff, 0x7a, 0x9a);
static constexpr uint16_t C_MOUTH   = rgb565(0x5a, 0x1e, 0x24);
static constexpr uint16_t C_HEART   = rgb565(0xff, 0x6b, 0x8a);
static constexpr uint16_t C_DROP    = rgb565(0x9f, 0xd6, 0xff);
static constexpr uint16_t C_ZZZ     = rgb565(0xe8, 0xec, 0xff);
static constexpr uint16_t C_PAPER   = rgb565(0xf4, 0xf1, 0xe8);
static constexpr uint16_t C_SHADOW  = rgb565(0x2e, 0x6e, 0x3a);
static constexpr uint16_t C_CUSHION = rgb565(0x7d, 0x58, 0x96);
static constexpr uint16_t C_CUSH_L  = rgb565(0x9c, 0x78, 0xb6);
static constexpr uint16_t C_PUFF    = rgb565(0xe6, 0xf0, 0xff);

static const float PI_F = 3.14159265f;
static const int FLOOR_IN = ROOM_FLOOR_Y - ROOM_CAT_Y;   // ground line inside the sprite

bool catDrawInit() {
    if (haveSprite) return true;
    spr.setColorDepth(16);
    haveSprite = spr.createSprite(CAT_W, CAT_H) != nullptr;
    if (!haveSprite) Serial.println("[dog] sprite alloc failed, drawing direct");
    return haveSprite;
}

// ---- small parts ---------------------------------------------------------------

static void thick(TFT_eSPI& g, int x0, int y0, int x1, int y1, int r, uint16_t c) {
    // Round-capped thick line (circles along it).
    int n = (int)(sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))) / 2) + 1;
    for (int i = 0; i <= n; i++)
        g.fillCircle(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, r, c);
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
    const int f = p.facing >= 0 ? 1 : -1;                 // +1 faces right
    const uint32_t t = p.nowMs;
    const bool cold = p.comfort == Comfort::COLD, hot = p.comfort == Comfort::HOT;

    int shiver = (idle && act == CatAct::SIT && cold) ? (((t / 70) % 2) ? 1 : -1) : 0;
    const int cx = ox + CAT_W / 2 + shiver;
    const int ground = oy + FLOOR_IN + 4;

    // Height: hop / petting bounce.
    int lift = 0;
    float petPhase = 0;
    if (p.mood == CatMood::PETTED) {
        uint32_t el = t - p.moodSince;
        petPhase = el >= PET_MS ? 1.0f : (float)el / (float)PET_MS;
        lift = (int)lroundf(5.0f * fabsf(sinf(petPhase * PI_F * 3.0f)));
    } else if (act == CatAct::HOP) {
        lift = (int)lroundf(14.0f * bump(ph));
    }
    const bool walking = act == CatAct::WALK;
    const int trot = walking ? (int)((t / 110) % 2) : 0;
    const float bow = act == CatAct::STRETCH ? bump(ph) : 0.0f;   // play bow: front down
    const bool scratch = act == CatAct::GROOM;

    const int by = ground - 16 - lift - (walking ? trot : 0);       // body centre y
    const int front = (int)lroundf(7.0f * bow);

    // Shadow.
    g.fillEllipse(cx, ground, 26 - lift / 2, 3, C_SHADOW);

    // Tail: wags; wild when petted, up during the play bow.
    {
        float sp = p.mood == CatMood::PETTED ? 45.0f : (walking ? 90.0f : 160.0f);
        int wag = (int)lroundf(6.0f * sinf((float)t / sp));
        int bx0 = cx - f * 22, by0 = by - 6;
        int tx = cx - f * (34 + (int)(4 * bow)), ty = by - 20 - (int)(6 * bow) + wag / 2;
        thick(g, bx0, by0, tx + f * wag / 2, ty, 3, C_FUR);
        g.fillCircle(tx + f * wag / 2, ty, 3, C_FUR_L);
    }

    // Legs (back pair behind, front pair), trotting alternately.
    {
        const int legH = 15;
        int lx[4] = { cx - f * 18, cx - f * 10, cx + f * 10, cx + f * 18 };
        for (int i = 0; i < 4; i++) {
            bool back = i < 2;
            int up = walking ? (((i + trot) % 2) ? 3 : 0) : 0;
            int top = by + 4 + (back ? 0 : front);
            int bot = ground - 1 - up;
            if (scratch && i == 0) continue;                // scratching leg drawn later
            uint16_t c = (i == 1 || i == 3) ? C_FUR : C_FUR_D;
            g.fillRoundRect(lx[i] - 3, top, 7, bot - top + 1, 3, c);
            g.fillEllipse(lx[i] + f, bot, 4, 2, C_FUR_L);
            (void)legH;
        }
    }

    // Body + belly.
    g.fillEllipse(cx, by + front / 2, 25, 13, C_FUR);
    g.fillEllipse(cx + f * 2, by + 6 + front / 2, 16, 5, C_FUR_L);
    g.fillEllipse(cx - f * 6, by - 5, 9, 4, C_FUR_D);             // back patch

    // Head position: forward, lower in the bow, turned while looking.
    int hx = cx + f * 22, hy = by - 18 + front + (int)(2 * bow);
    int lookOff = 0;
    if (act == CatAct::LOOK) lookOff = (int)lroundf(3.0f * p.look * hold(ph));
    if (scratch) hy += 2;

    // Collar (or scarf when cold), in the comfort colour.
    if (cold) {
        g.fillRoundRect(hx - f * 10 - 8, hy + 8, 18, 7, 3, p.body);
        g.fillRoundRect(hx - f * 12 - 2, hy + 12, 6, 12, 2, mix565(p.body, 0x0000, 200));
    } else {
        thick(g, hx - f * 9, hy + 6, hx - f * 4, hy + 12, 2, p.body);
        g.fillCircle(hx - f * 4, hy + 15, 2, C_PAPER);
    }

    // Head.
    g.fillCircle(hx, hy, 14, C_FUR);
    g.fillEllipse(hx + f * 10, hy + 5, 9, 7, C_FUR_L);              // muzzle
    g.fillCircle(hx + f * 18, hy + 2, 3, C_NOSE);
    g.drawPixel(hx + f * 17, hy + 1, C_FUR_L);
    // Ear: floppy; perks up while looking or petted.
    bool perk = act == CatAct::LOOK || p.mood == CatMood::PETTED;
    if (perk) g.fillTriangle(hx - f * 4, hy - 10, hx - f * 13, hy - 8, hx - f * 10, hy - 22, C_FUR_D);
    else      g.fillEllipse(hx - f * 8, hy + 2, 5, 11, C_FUR_D);

    // Eyes.
    const int ex = hx + f * 5 + lookOff, ey = hy - 4;
    const bool yawn = act == CatAct::YAWN;
    if (p.mood == CatMood::PETTED) {                      // happy ^
        g.drawLine(ex - 3, ey + 1, ex, ey - 2, C_EYE);
        g.drawLine(ex, ey - 2, ex + 3, ey + 1, C_EYE);
        g.drawLine(ex - 3, ey + 2, ex, ey - 1, C_EYE);
        g.drawLine(ex, ey - 1, ex + 3, ey + 2, C_EYE);
    } else if (p.blink || yawn || scratch) {
        g.fillRect(ex - 3, ey, 7, 2, C_EYE);
    } else if (p.mood == CatMood::WOKEN) {
        g.fillRoundRect(ex - 3, ey, 7, 3, 1, C_EYE);
    } else {
        g.fillCircle(ex, ey, 3, C_EYE);
        g.drawPixel(ex + 1, ey - 1, C_PAPER);
    }
    // Brow + cheek.
    g.fillCircle(hx + f * 1, hy + 6, 2, mix565(C_TONGUE, C_FUR, 90));

    // Mouth: yawn, panting tongue, letter, or a smile.
    const int mx = hx + f * 12, my = hy + 10;
    if (yawn) {
        int open = 2 + (int)lroundf(5.0f * bump(ph));
        g.fillEllipse(mx, my + 1, 4, open, C_MOUTH);
        g.fillEllipse(mx, my + open - 1, 3, 2, C_TONGUE);
    } else if (p.envelope) {
        int ly = my - 2 + (int)((t / 300) % 2);
        g.fillRect(mx + f * 2 - 8, ly, 16, 11, C_PAPER);
        g.drawRect(mx + f * 2 - 8, ly, 16, 11, C_FUR_D);
        g.drawLine(mx + f * 2 - 8, ly, mx + f * 2, ly + 6, C_FUR_D);
        g.drawLine(mx + f * 2 + 7, ly, mx + f * 2, ly + 6, C_FUR_D);
        g.fillCircle(mx + f * 2, ly + 6, 1, C_HEART);
    } else if (hot || p.mood == CatMood::PETTED) {
        int pant = hot ? (int)((t / 160) % 2) : 0;
        g.fillEllipse(mx, my + 3 + pant, 3, 4 + pant, C_TONGUE);
        g.drawFastVLine(mx, my + 3, 4 + pant, mix565(C_MOUTH, C_TONGUE, 120));
    } else {
        g.drawLine(mx - f * 5, my, mx - f * 2, my + 2, C_NOSE);
        g.drawLine(mx - f * 2, my + 2, mx + f * 1, my, C_NOSE);
    }

    // Scratching hind leg, fast.
    if (scratch) {
        int jig = ((t / 60) % 2) ? 2 : -2;
        thick(g, cx - f * 16, by + 6, hx - f * 10, hy + 8 + jig, 3, C_FUR_D);
        g.fillCircle(hx - f * 10, hy + 8 + jig, 4, C_FUR_L);
    }

    // Comfort / humidity extras.
    if (cold && (t / 900) % 2 == 0) {                     // breath puff
        int k = (int)((t / 150) % 6);
        g.fillCircle(hx + f * (22 + k), hy + 6 - k, 2 + k / 2, C_PUFF);
    }
    if (hot) {
        for (int i = 0; i < 2; i++) {
            int k = (int)(((t + i * 500) / 120) % 8);
            g.fillCircle(hx - f * (4 + i * 8), hy - 18 - k, 1, C_DROP);
        }
    }
    if (p.humid == HumidMood::HUMID) drop(g, hx - f * 14, hy - 14 + (int)((t / 150) % 6));
    if (p.mood == CatMood::PETTED) heart(g, hx + f * 6, oy + 10 - (int)(petPhase * 12.0f), 3, C_HEART);
}

// ---- asleep: curled on its cushion ---------------------------------------------------

static void drawAsleep(TFT_eSPI& g, const CatPose& p, int ox, int oy) {
    const uint32_t t = p.nowMs;
    const int cx = ox + CAT_W / 2, ground = oy + FLOOR_IN + 4;
    const uint16_t fur = mix565(C_FUR, 0x0000, 190), furD = mix565(C_FUR_D, 0x0000, 190);
    const uint16_t furL = mix565(C_FUR_L, 0x0000, 190);

    g.fillEllipse(cx, ground - 3, 40, 8, C_CUSHION);
    g.fillEllipse(cx, ground - 5, 34, 5, C_CUSH_L);

    float br = sinf((float)(t % 3600) * (2.0f * PI_F / 3600.0f));
    const int ry = 13 + (int)lroundf(1.5f * br);
    g.fillEllipse(cx - 2, ground - 16, 28, ry, fur);                 // curled body
    g.fillEllipse(cx - 10, ground - 22, 10, 4, furD);
    g.fillEllipse(cx + 2, ground - 7, 22, 4, furL);                  // tail wrapped round
    // Head resting on the paws, eyes closed.
    const int hx = cx + 20, hy = ground - 15;
    g.fillEllipse(hx + 4, hy + 6, 10, 4, furL);                      // paws
    g.fillCircle(hx, hy, 12, fur);
    g.fillEllipse(hx + 8, hy + 4, 8, 5, furL);
    g.fillCircle(hx + 14, hy + 2, 2, C_NOSE);
    g.fillEllipse(hx - 7, hy + 2, 4, 9, furD);                       // ear
    for (int k = 0; k < 2; k++) {                                    // closed eye "u"
        g.drawLine(hx + 1, hy - 3 + k, hx + 3, hy - 1 + k, C_EYE);
        g.drawLine(hx + 3, hy - 1 + k, hx + 6, hy - 3 + k, C_EYE);
    }
    g.fillCircle(hx - 2, hy + 6, 2, p.body);                         // collar tag
    zzz(g, hx - 6, hy - 18, t, rgb565(0x0e, 0x18, 0x3a));
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
