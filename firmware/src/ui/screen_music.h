// firmware/src/ui/screen_music.h -- Now Playing: backend-composed backdrop
// (blurred cover + sharp cover), title, artist with an equaliser, progress.
#pragma once
#include <stdint.h>
#include "../nowplaying.h"

// The composed artwork JPEG (backend/artwork.py) covers the body below the
// header: MUSIC_IMG_W x MUSIC_IMG_H at (0, HEADER_H).
#define MUSIC_IMG_W 320
#define MUSIC_IMG_H 200

// Background of the whole music screen (header and page dots included); the
// composed image's text area is this exact colour.
uint16_t musicBg(const NowPlaying& np);
// Full body. art/artLen: the composed JPEG for this song (null until it
// arrives; a placeholder backdrop is drawn instead). progressMs: estimate.
void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen, int progressMs);
// Only the progress row (bar + times), when the duration is known.
void drawMusicProgress(const NowPlaying& np, int progressMs);
// Equaliser bars next to the artist (call every ~90 ms).
void drawMusicEq(const NowPlaying& np, uint32_t nowMs);
