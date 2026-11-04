// firmware/src/nowplaying.h
#pragma once
#include <Arduino.h>
#include "config.h"

struct NowPlaying {
    bool  is_playing;
    char  title[64];
    char  artist[64];
    char  album[64];
    char  artwork_url[256];
    int   progress_ms;
    int   duration_ms;
    bool     hasAccent;   // backend sent the cover's main colour
    uint16_t accent;      // RGB565
    bool     hasBg;       // backend sent the solid colour under the text
    uint16_t bg;          // RGB565 (matches the composed artwork's text area)
};

