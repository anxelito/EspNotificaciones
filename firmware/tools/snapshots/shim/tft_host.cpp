// Host implementation of the TFT_eSPI shim (see TFT_eSPI.h).
#include <TFT_eSPI.h>
#include <algorithm>

#include "glcdfont.c"   // Adafruit 5x7 font from the TFT_eSPI library (-I .pio/.../Fonts)
static uint8_t font_data_glcd(uint32_t c, int col) { return font[c * 5 + col]; }

HostSerial Serial;
static uint32_t g_millis = 0;
uint32_t millis() { return g_millis; }
void hostSetMillis(uint32_t ms) { g_millis = ms; }

TFT_eSPI::TFT_eSPI(int w, int h) : w_(w), h_(h), buf_((size_t)w * h, 0), vw_(w), vh_(h) {}

// ---- pixels -------------------------------------------------------------------

void TFT_eSPI::plot(int32_t x, int32_t y, uint16_t c) {
    x += ox_; y += oy_;
    if (x < vx_ || y < vy_ || x >= vx_ + vw_ || y >= vy_ + vh_) return;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    buf_[(size_t)y * w_ + x] = c;
}

void TFT_eSPI::drawPixel(int32_t x, int32_t y, uint32_t c) { plot(x, y, (uint16_t)c); }

uint16_t TFT_eSPI::readPixel(int32_t x, int32_t y) {
    x += ox_; y += oy_;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return 0;
    return buf_[(size_t)y * w_ + x];
}

void TFT_eSPI::span(int32_t x0, int32_t x1, int32_t y, uint16_t c) {
    if (x1 < x0) std::swap(x0, x1);
    for (int32_t x = x0; x <= x1; x++) plot(x, y, c);
}

void TFT_eSPI::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) plot(x + i, y + j, (uint16_t)c);
}

void TFT_eSPI::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    drawFastHLine(x, y, w, c);
    drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c);
    drawFastVLine(x + w - 1, y, h, c);
}

// Half-width of a filled circle row, matching TFT_eSPI's midpoint fill.
static int32_t circleHalf(int32_t r, int32_t dy) {
    int32_t dx = 0;
    while ((dx + 1) * (dx + 1) + dy * dy <= r * r + r) dx++;
    return dx;
}

void TFT_eSPI::fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c) {
    if (r < 0) return;
    for (int32_t dy = -r; dy <= r; dy++) {
        int32_t dx = circleHalf(r, dy);
        span(x - dx, x + dx, y + dy, (uint16_t)c);
    }
}

void TFT_eSPI::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c) {
    int32_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    plot(x0, y0 + r, c); plot(x0, y0 - r, c); plot(x0 + r, y0, c); plot(x0 - r, y0, c);
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        plot(x0 + x, y0 + y, c); plot(x0 - x, y0 + y, c); plot(x0 + x, y0 - y, c); plot(x0 - x, y0 - y, c);
        plot(x0 + y, y0 + x, c); plot(x0 - y, y0 + x, c); plot(x0 + y, y0 - x, c); plot(x0 - y, y0 - x, c);
    }
}

void TFT_eSPI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    r = std::min(r, std::min(w, h) / 2);
    for (int32_t j = 0; j < h; j++) {
        int32_t inset = 0;
        int32_t dy = -1;
        if (j < r) dy = r - j;
        else if (j >= h - r) dy = j - (h - r - 1);
        if (dy >= 0) inset = r - circleHalf(r, dy);
        span(x + inset, x + w - 1 - inset, y + j, (uint16_t)c);
    }
}

void TFT_eSPI::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
    // Straight sides + sampled corner arcs; good enough for previews.
    drawFastHLine(x + r, y, w - 2 * r, c);
    drawFastHLine(x + r, y + h - 1, w - 2 * r, c);
    drawFastVLine(x, y + r, h - 2 * r, c);
    drawFastVLine(x + w - 1, y + r, h - 2 * r, c);
    for (int32_t a = 0; a <= 90; a += 3) {
        float t = a * 3.14159265f / 180.0f;
        int32_t dx = (int32_t)lroundf(cosf(t) * r), dy = (int32_t)lroundf(sinf(t) * r);
        plot(x + r - dx, y + r - dy, c);
        plot(x + w - 1 - r + dx, y + r - dy, c);
        plot(x + r - dx, y + h - 1 - r + dy, c);
        plot(x + w - 1 - r + dx, y + h - 1 - r + dy, c);
    }
}

void TFT_eSPI::fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t c) {
    if (rx < 1 || ry < 1) { span(x - rx, x + rx, y, c); return; }
    for (int32_t dy = -ry; dy <= ry; dy++) {
        float f = 1.0f - (float)(dy * dy) / (float)(ry * ry);
        int32_t dx = (int32_t)lroundf(rx * sqrtf(std::max(0.0f, f)));
        span(x - dx, x + dx, y + dy, c);
    }
}

void TFT_eSPI::drawEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t c) {
    if (rx < 1 || ry < 1) return;
    int32_t px = -1, py = -1;
    for (int32_t dy = 0; dy <= ry; dy++) {
        int32_t dx = (int32_t)lroundf(rx * sqrtf(std::max(0.0f, 1.0f - (float)(dy * dy) / (ry * ry))));
        plot(x + dx, y + dy, c); plot(x - dx, y + dy, c); plot(x + dx, y - dy, c); plot(x - dx, y - dy, c);
        (void)px; (void)py;
    }
    for (int32_t dx = 0; dx <= rx; dx++) {
        int32_t dy = (int32_t)lroundf(ry * sqrtf(std::max(0.0f, 1.0f - (float)(dx * dx) / (rx * rx))));
        plot(x + dx, y + dy, c); plot(x - dx, y + dy, c); plot(x + dx, y - dy, c); plot(x - dx, y - dy, c);
    }
}

void TFT_eSPI::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) {
    int32_t dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    for (;;) {
        plot(x0, y0, (uint16_t)c);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void TFT_eSPI::fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c) {
    // Sort by y, then scanline-fill between edges (inclusive, like TFT_eSPI).
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y2, y1); std::swap(x2, x1); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y0 == y2) {
        span(std::min({x0, x1, x2}), std::max({x0, x1, x2}), y0, (uint16_t)c);
        return;
    }
    for (int32_t y = y0; y <= y2; y++) {
        float xa = x0 + (float)(x2 - x0) * (y - y0) / (float)(y2 - y0);
        float xb;
        if (y < y1 || y1 == y2) xb = (y1 == y0) ? x1 : x0 + (float)(x1 - x0) * (y - y0) / (float)(y1 - y0);
        else                    xb = x1 + (float)(x2 - x1) * (y - y1) / (float)(y2 - y1);
        span((int32_t)lroundf(xa), (int32_t)lroundf(xb), y, (uint16_t)c);
    }
}

void TFT_eSPI::drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c) {
    drawLine(x0, y0, x1, y1, c); drawLine(x1, y1, x2, y2, c); drawLine(x2, y2, x0, y0, c);
}

void TFT_eSPI::setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool vpDatum) {
    vx_ = x; vy_ = y; vw_ = w; vh_ = h;
    ox_ = vpDatum ? x : 0; oy_ = vpDatum ? y : 0;
}

void TFT_eSPI::resetViewport() { vx_ = vy_ = ox_ = oy_ = 0; vw_ = w_; vh_ = h_; }

// ---- smooth fonts (VLW) ---------------------------------------------------------

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

void TFT_eSPI::loadFont(const uint8_t* f) {
    glyphs_.clear();
    uint32_t n = be32(f);
    ascent_ = (int)be32(f + 16);
    descent_ = (int)be32(f + 20);
    maxAscent_ = ascent_; maxDescent_ = descent_;
    const uint8_t* rec = f + 24;
    const uint8_t* bmp = rec + n * 28;
    for (uint32_t i = 0; i < n; i++, rec += 28) {
        Glyph g;
        g.u = be32(rec); g.h = (int)be32(rec + 4); g.w = (int)be32(rec + 8);
        g.adv = (int)be32(rec + 12); g.dy = (int32_t)be32(rec + 16); g.dx = (int32_t)be32(rec + 20);
        g.bmp = bmp; bmp += g.w * g.h;
        if ((g.u > 0xFF) || ((g.u > 0x20) && (g.u < 0xA0) && (g.u != 0x7F))) {
            if (g.dy > maxAscent_ && ((g.u > 0x20 && g.u < 0x7F) || g.u > 0xA0)) maxAscent_ = g.dy;
            if (g.h - g.dy > maxDescent_) maxDescent_ = g.h - g.dy;
        }
        glyphs_.push_back(g);
    }
    yAdvance_ = maxAscent_ + maxDescent_;
    spaceW_ = (ascent_ + descent_) * 2 / 7;
    fontLoaded_ = true;
}

int TFT_eSPI::glyphIndex(uint32_t u) const {
    for (size_t i = 0; i < glyphs_.size(); i++) if (glyphs_[i].u == u) return (int)i;
    return -1;
}

static uint32_t nextUtf8(const char*& s) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *s) return ((c & 0x1F) << 6) | ((uint8_t)*s++ & 0x3F);
    if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
        uint32_t u = ((c & 0x0F) << 12) | (((uint8_t)s[0] & 0x3F) << 6) | ((uint8_t)s[1] & 0x3F);
        s += 2;
        return u;
    }
    return c;
}

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t a) {
    int r = (((fg >> 11) & 31) * a + ((bg >> 11) & 31) * (255 - a)) / 255;
    int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
    int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
    return (uint16_t)(r << 11 | g << 5 | b);
}

void TFT_eSPI::drawGlyph(int idx, int32_t& cx, int32_t cy) {
    const Glyph& g = glyphs_[idx];
    int32_t y0 = cy + maxAscent_ - g.dy, x0 = cx + g.dx;
    for (int j = 0; j < g.h; j++)
        for (int i = 0; i < g.w; i++) {
            uint8_t a = g.bmp[j * g.w + i];
            if (!a) continue;
            uint16_t under = (fg_ == bg_) ? readPixel(x0 + i, y0 + j) : bg_;
            plot(x0 + i, y0 + j, a == 255 ? fg_ : blend565(fg_, under, a));
        }
    cx += g.adv;
}

static int builtinScale(uint8_t font) { return font == 4 ? 3 : font == 2 ? 2 : 1; }

int16_t TFT_eSPI::textWidth(const char* s, uint8_t font) {
    if (fontLoaded_ && font == 0) {
        int w = 0;
        while (*s) {
            uint32_t u = nextUtf8(s);
            if (u == 0x20) { w += spaceW_; continue; }
            int i = glyphIndex(u);
            if (i < 0) { w += spaceW_ + 1; continue; }
            if (w == 0 && glyphs_[i].dx < 0) w -= glyphs_[i].dx;
            w += *s ? glyphs_[i].adv : (glyphs_[i].dx + glyphs_[i].w);
        }
        return (int16_t)w;
    }
    int n = 0;
    while (*s) { nextUtf8(s); n++; }
    return (int16_t)(n * 6 * builtinScale(font ? font : 1));
}

int16_t TFT_eSPI::fontHeight(int16_t font) {
    if (fontLoaded_ && font == 0) return (int16_t)yAdvance_;
    return (int16_t)(8 * builtinScale(font ? (uint8_t)font : 1));
}

int16_t TFT_eSPI::drawString(const char* s, int32_t x, int32_t y, uint8_t font) {
    int32_t w = textWidth(s, font), h = fontHeight(font);
    switch (datum_) {
        case TC_DATUM: x -= w / 2; break;
        case TR_DATUM: x -= w; break;
        case ML_DATUM: y -= h / 2; break;
        case MC_DATUM: x -= w / 2; y -= h / 2; break;
        case MR_DATUM: x -= w; y -= h / 2; break;
        case BC_DATUM: x -= w / 2; y -= h; break;
        case BR_DATUM: x -= w; y -= h; break;
        case BL_DATUM: y -= h; break;
        default: break;
    }
    if (fontLoaded_ && font == 0) {
        int32_t cx = x;
        while (*s) {
            uint32_t u = nextUtf8(s);
            if (u == 0x20) { cx += spaceW_; continue; }
            int i = glyphIndex(u);
            if (i < 0) { cx += spaceW_ + 1; continue; }
            drawGlyph(i, cx, y);
        }
        return (int16_t)w;
    }
    // GLCD 5x7, scaled for fonts 2 / 4 (only the Zzz uses those).
    int sc = builtinScale(font ? font : 1), cx = x;
    while (*s) {
        uint32_t u = nextUtf8(s);
        if (u > 0xFF) u = '?';
        for (int col = 0; col < 5; col++) {
            uint8_t bits = font_data_glcd(u, col);
            for (int row = 0; row < 8; row++)
                if (bits & (1 << row)) fillRect(cx + col * sc, y + row * sc, sc, sc, fg_);
                else if (fg_ != bg_)   fillRect(cx + col * sc, y + row * sc, sc, sc, bg_);
        }
        cx += 6 * sc;
    }
    return (int16_t)w;
}

// ---- sprite ------------------------------------------------------------------------

void* TFT_eSprite::createSprite(int w, int h) {
    w_ = w; h_ = h;
    buf_.assign((size_t)w * h, 0);
    resetViewport();
    return buf_.data();
}

void TFT_eSprite::pushSprite(int32_t x, int32_t y) {
    for (int j = 0; j < h_; j++)
        for (int i = 0; i < w_; i++) parent_->drawPixel(x + i, y + j, buf_[(size_t)j * w_ + i]);
}
