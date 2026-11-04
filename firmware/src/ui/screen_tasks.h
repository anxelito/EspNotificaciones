// firmware/src/ui/screen_tasks.h
#pragma once
#include "../tasks.h"

// Draws the body only (between header and page dots): soft cards, "+N mas".
void drawTasksScreen(const TaskItem* tasks, int count);
