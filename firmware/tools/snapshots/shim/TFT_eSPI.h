// Host shim of TFT_eSPI 2.5.x: the drawing calls the UI uses, rendered into
// an RGB565 framebuffer so tools/snapshots can save real panel screenshots.
// Smooth (VLW) fonts are parsed like TFT_eSPI's Smooth_font.cpp; built-in
// font 1 is the GLCD 5x7 font, fonts 2 and 4 are it scaled x2 / x3.
#pragma once
#include <Arduino.h>
#include <vector>

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8
#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF

class TFT_eSPI {
public:
    TFT_eSPI(int w = 320, int h = 240);
    virtual ~TFT_eSPI() {}

    void init() {}
    void setRotation(int) {}
    int16_t width() const { return (int16_t)w_; }
    int16_t height() const { return (int16_t)h_; }

    void drawPixel(int32_t x, int32_t y, uint32_t c);
    uint16_t readPixel(int32_t x, int32_t y);
    void fillScreen(uint32_t c) { fillRect(0, 0, w_, h_, c); }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) { fillRect(x, y, w, 1, c); }
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t c) { fillRect(x, y, 1, h, c); }
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c);
    void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t c);
    void fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t c);
    void drawEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t c);
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c);
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c);
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c);

    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool vpDatum = true);
    void resetViewport();

    void setTextColor(uint16_t c) { fg_ = bg_ = c; }
    void setTextColor(uint16_t c, uint16_t b, bool = false) { fg_ = c; bg_ = b; }
    void setTextDatum(uint8_t d) { datum_ = d; }
    void setTextSize(uint8_t) {}
    int16_t drawString(const char* s, int32_t x, int32_t y, uint8_t font);
    int16_t drawString(const char* s, int32_t x, int32_t y) { return drawString(s, x, y, 0); }
    int16_t drawString(const String& s, int32_t x, int32_t y, uint8_t font = 0) { return drawString(s.c_str(), x, y, font); }
    int16_t textWidth(const char* s, uint8_t font);
    int16_t textWidth(const char* s) { return textWidth(s, 0); }
    int16_t textWidth(const String& s) { return textWidth(s.c_str(), 0); }
    int16_t fontHeight(int16_t font = 0);
    void loadFont(const uint8_t* vlw);
    void unloadFont() { fontLoaded_ = false; }

    // Host only.
    std::vector<uint16_t>& pixels() { return buf_; }

protected:
    int w_, h_;
    std::vector<uint16_t> buf_;

private:
    void span(int32_t x0, int32_t x1, int32_t y, uint16_t c);   // inclusive
    void plot(int32_t x, int32_t y, uint16_t c);
    int  glyphIndex(uint32_t u) const;
    void drawGlyph(int idx, int32_t& cx, int32_t cy);

    // viewport (absolute clip) + datum offset
    int32_t vx_ = 0, vy_ = 0, vw_, vh_, ox_ = 0, oy_ = 0;
    uint16_t fg_ = 0xFFFF, bg_ = 0;
    uint8_t datum_ = TL_DATUM;

    struct Glyph { uint32_t u; int h, w, adv, dy, dx; const uint8_t* bmp; };
    bool fontLoaded_ = false;
    std::vector<Glyph> glyphs_;
    int ascent_ = 0, descent_ = 0, maxAscent_ = 0, maxDescent_ = 0, yAdvance_ = 0, spaceW_ = 0;
};

class TFT_eSprite : public TFT_eSPI {
public:
    explicit TFT_eSprite(TFT_eSPI* parent) : TFT_eSPI(0, 0), parent_(parent) {}
    void setColorDepth(int) {}
    void* createSprite(int w, int h);
    void deleteSprite() { buf_.clear(); w_ = h_ = 0; }
    void pushSprite(int32_t x, int32_t y);
private:
    TFT_eSPI* parent_;
};
