// firmware/src/ui/text_fit.h -- UTF-8-safe text fitting / wrapping (smooth fonts)
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "fonts.h"

// Copies src to out, truncating (never inside a UTF-8 sequence) and appending
// "..." until it fits maxPx wide in font f.
void fitText(const char* src, char* out, size_t outLen, int maxPx, UiFont f);

// Word-wraps src into at most maxLines lines of maxPx width. Words longer
// than a line are split. If text remains after the last line, that line ends
// in "...". Returns the number of lines the text needs (<= maxLines).
int wrapLineCount(const char* src, int maxPx, UiFont f, int maxLines);

// Draws src wrapped as above, top of the first line at y, lineH px per line,
// glyph edges blended into bg. strike draws a strike-through on each line;
// center treats x as the horizontal centre of each line. Returns lines drawn.
int drawWrapped(const char* src, int x, int y, int maxPx, UiFont f, int lineH,
                int maxLines, uint16_t fg, uint16_t bg, bool strike = false,
                bool center = false);

// Single line helpers (font f, edges blended into bg).
void drawText(const char* s, int x, int y, UiFont f, uint16_t fg, uint16_t bg);
void drawTextCentre(const char* s, int cx, int y, UiFont f, uint16_t fg, uint16_t bg);
void drawTextRight(const char* s, int rx, int y, UiFont f, uint16_t fg, uint16_t bg);
int  textWidthIn(const char* s, UiFont f);
