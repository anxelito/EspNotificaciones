// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SYNC_INTERVAL   3600000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     32
#define TASK_H       64
#define TASK_MARGIN   4
#define BAR_W         4
