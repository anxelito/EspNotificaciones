// firmware/src/tasks.h
#pragma once
#include <Arduino.h>
#include "config.h"

struct TaskItem {
    int  id;
    char text[64];
    char priority[8];     // "normal" | "urgent"
    char status[8];       // "pending" | "done"
    char time_display[16]; // "14:30 16/05"
};

struct CalEvent {
    char    summary[48];
    char    time_display[12];
    uint8_t day;  // 0 = today, 1 = tomorrow
};

// Sort: urgent-pending first, normal-pending second, done last
void sortTasks(TaskItem* tasks, int count);

// Returns true on success
bool saveTasks(const TaskItem* tasks, int count);

// Returns number of tasks loaded (0 if file missing/corrupt)
int  loadTasks(TaskItem* tasks);
