// firmware/src/ui/cat_draw.h -- animated cat mascot (Habitacion screen)
#pragma once
#include <stdint.h>
#include "../logic/cat_state.h"
#include "../logic/cat_behavior.h"
#include "../logic/room.h"

#define CAT_W 100
#define CAT_H 90

struct CatPose {
    CatMood   mood;
    bool      blink;      // catBlinking(state, now)
    HumidMood humid;      // sweat drop (HUMID) / open mouth + tongue (DRY)
    uint16_t  body;       // RGB565 body colour (comfortColor); raw colour; catDraw darkens it when SLEEPING
    uint32_t  nowMs;      // drives Zzz drift, breathing, ear twitch
    uint32_t  moodSince;  // CatState.since: start of the PETTED hop (extension)
    // Idle behaviour (awake only; ignored while asleep)
    CatAct    act      = CatAct::SIT;
    float     actPhase = 0;   // behaviorPhase(), 0..1
    int8_t    facing   = 1;   // -1 left, +1 right
    int8_t    look     = 0;   // LOOK direction
    Comfort   comfort  = Comfort::UNKNOWN;  // cold: shiver, hot: pant
    bool      envelope = false;  // holding a letter (new task just arrived)
};

// Creates the CAT_W x CAT_H 16-bit sprite once (~19 KB). If allocation fails,
// catDraw falls back to drawing straight onto tft (may flicker). Returns
// true if the sprite exists.
bool catDrawInit();
// Renders one frame into the sprite and pushes it with its top-left at (x,y).
// The room scene behind the cat (room_scene) is painted into the sprite first.
void catDraw(const CatPose& p, int x, int y);
