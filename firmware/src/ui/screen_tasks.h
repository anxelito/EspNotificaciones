// firmware/src/ui/screen_tasks.h
#pragma once
#include <stdint.h>
#include "../tasks.h"

// Draws the body only (between header and page dots): progress bar under the
// header, soft cards, "+N más".
void drawTasksScreen(const TaskItem* tasks, int count);
// Animates the dots of urgent cards drawn by the last drawTasksScreen.
void drawTasksPulse(uint32_t nowMs);
