// firmware/src/ui/cat_draw.h -- animated cat mascot (Habitacion screen)
#pragma once
#include <stdint.h>
#include "../logic/cat_state.h"
#include "../logic/room.h"

#define CAT_W 100
#define CAT_H 90

struct CatPose {
    CatMood   mood;
    bool      blink;      // catBlinking(state, now)
    HumidMood humid;      // sweat drop (HUMID) / open mouth + tongue (DRY)
    uint16_t  body;       // RGB565 body colour (comfortColor); darkened when SLEEPING
    uint32_t  nowMs;      // drives Zzz drift, breathing, ear twitch
    uint32_t  moodSince;  // CatState.since: start of the PETTED hop (extension)
};

// Creates the CAT_W x CAT_H 16-bit sprite once (~18 KB). If allocation fails,
// catDraw falls back to drawing straight onto tft (may flicker). Returns
// true if the sprite exists.
bool catDrawInit();
// Renders one frame into the sprite and pushes it with its top-left at (x,y).
void catDraw(const CatPose& p, int x, int y);
