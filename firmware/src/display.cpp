// firmware/src/display.cpp
#include "display.h"

TFT_eSPI tft = TFT_eSPI();

#include "ui/theme.h"

void displayInit() {
    tft.init();
    tft.setRotation(1);  // landscape
    tft.fillScreen(C_BG);
}

void drawSplash(const String& msg) {
    tft.fillScreen(C_BG);
    tft.setTextColor(TFT_CYAN, C_BG);
    tft.drawCentreString("ESP32 TASKS", SCREEN_W / 2, 90, 4);
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString(msg, SCREEN_W / 2, 130, 2);
}

// --- Now Playing ---

// TJpg_Decoder callback: push each decoded MCU block to the TFT
static bool _jpgRenderCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    tft.pushImage(x, y, w, h, bitmap);
    return true;  // continue decoding
}

void renderArtwork(const uint8_t* jpegBuf, int jpegLen, int x, int y) {
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(_jpgRenderCallback);
    TJpgDec.setJpgScale(1);  // 1:1 scale: backend already sends ARTWORK_SIZE px
    TJpgDec.drawJpg(x, y, jpegBuf, jpegLen);
}
