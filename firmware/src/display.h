// firmware/src/display.h
#pragma once
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "tasks.h"
#include "nowplaying.h"
#include "config.h"

extern TFT_eSPI tft;

void displayInit();
void drawSplash(const String& msg);

// Downloads artwork JPEG into buffer, decodes and renders at (x,y)
void renderArtwork(const uint8_t* jpegBuf, int jpegLen, int x, int y);
