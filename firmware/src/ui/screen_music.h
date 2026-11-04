// firmware/src/ui/screen_music.h -- restyled Now Playing screen
#pragma once
#include <stdint.h>
#include "../nowplaying.h"

// Artwork top-left corner (screen coordinates), for renderArtwork().
extern const int MUSIC_ART_X;
extern const int MUSIC_ART_Y;

// Full body (between header and page dots). Draws a placeholder where the
// artwork goes; if art/artLen are valid the JPEG is decoded over it.
void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen);
// Only the progress bar + elapsed/total times (no flicker).
void drawMusicProgress(const NowPlaying& np);
