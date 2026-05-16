// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SYNC_INTERVAL   3600000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     28
#define TASK_H       38
#define TASK_MARGIN   3
#define BAR_W         3
