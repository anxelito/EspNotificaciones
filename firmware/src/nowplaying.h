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
};

