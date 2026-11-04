// firmware/src/ui/text_fit.h -- shared UTF-8-safe text fitting / wrapping
#pragma once
#include <stddef.h>
#include <stdint.h>

// Copies src to out, truncating (never inside a UTF-8 sequence) and appending
// "..." until it fits maxPx wide in the given TFT_eSPI font.
void fitText(const char* src, char* out, size_t outLen, int maxPx, int font);

// Word-wraps src into at most maxLines lines of maxPx width. Words longer
// than a line are split. If text remains after the last line, that line ends
// in "...". Returns the number of lines the text needs (<= maxLines).
int wrapLineCount(const char* src, int maxPx, int font, int maxLines);

// Draws src wrapped as above, top-left at (x, y), lineH px per line, in the
// current text colours. strike draws a strike-through on each line.
// Returns the number of lines drawn.
int drawWrapped(const char* src, int x, int y, int maxPx, int font, int lineH,
                int maxLines, uint16_t fg, uint16_t bg, bool strike = false);
