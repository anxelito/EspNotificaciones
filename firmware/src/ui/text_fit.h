// firmware/src/ui/text_fit.h -- shared UTF-8-safe text truncation
#pragma once
#include <stddef.h>

// Copies src to out, truncating (never inside a UTF-8 sequence) and appending
// "~" until it fits maxPx wide in the given TFT_eSPI font.
void fitText(const char* src, char* out, size_t outLen, int maxPx, int font);
