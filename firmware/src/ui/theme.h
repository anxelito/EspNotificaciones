// firmware/src/ui/theme.h -- soft-card palette (RGB565) and comfort helpers
#pragma once
#include <stdint.h>
#include "../config.h"        // HEADER_H lives here (single definition)
#include "../logic/room.h"

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t C_BG      = rgb565(0x0b, 0x0d, 0x12);  // 0x0862
constexpr uint16_t C_CARD    = rgb565(0x16, 0x1a, 0x24);  // 0x10C4
constexpr uint16_t C_MUTED   = rgb565(0x6b, 0x73, 0x85);  // 0x6B90
constexpr uint16_t C_TEXT    = rgb565(0xe8, 0xe8, 0xe8);  // 0xEF5D
constexpr uint16_t C_COLD    = rgb565(0x5a, 0xb0, 0xff);  // 0x5D9F
constexpr uint16_t C_OK      = rgb565(0x4f, 0xd1, 0x8b);  // 0x4E91
constexpr uint16_t C_HOT     = rgb565(0xff, 0x8a, 0x3d);  // 0xFC47
constexpr uint16_t C_URGENT  = rgb565(0xff, 0x5c, 0x5c);  // 0xFAEB
// extras used by chrome / tasks
constexpr uint16_t C_URGENT_BG = rgb565(0x3a, 0x1c, 0x1f); // 0x38E3
constexpr uint16_t C_URGENT_FG = rgb565(0xff, 0x8a, 0x8a); // 0xFC51
constexpr uint16_t C_TIME      = rgb565(0xcf, 0xd3, 0xdc); // 0xCE9B
constexpr uint16_t C_DOT_OFF   = rgb565(0x2a, 0x30, 0x40); // 0x2988

constexpr int CARD_R = 8;
constexpr int MARGIN = 8;

inline uint16_t comfortColor(Comfort c) {
    switch (c) {
        case Comfort::COLD: return C_COLD;
        case Comfort::HOT:  return C_HOT;
        default:            return C_OK;   // OK and UNKNOWN
    }
}

inline const char* comfortWord(Comfort c) {
    switch (c) {
        case Comfort::COLD: return "fresco";
        case Comfort::OK:   return "bien";
        case Comfort::HOT:  return "calor";
        default:            return "--";
    }
}

// Mix two RGB565 colours: alpha 255 -> a, 0 -> b (per channel, integer).
constexpr uint16_t mix565(uint16_t a, uint16_t b, uint8_t alpha) {
    return (uint16_t)(
        ((((a >> 11) & 0x1F) * alpha + ((b >> 11) & 0x1F) * (255 - alpha)) / 255) << 11 |
        ((((a >> 5) & 0x3F) * alpha + ((b >> 5) & 0x3F) * (255 - alpha)) / 255) << 5 |
        (((a & 0x1F) * alpha + (b & 0x1F) * (255 - alpha)) / 255));
}
// ~50 % darker (used for the sleeping cat's body).
constexpr uint16_t darken565(uint16_t c) { return mix565(c, 0x0000, 128); }
