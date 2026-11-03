// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SYNC_INTERVAL   3600000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Now Playing
#define NOWPLAYING_POLL_MS  3000UL    // poll Spotify every 3s
#define ARTWORK_SIZE        64        // 64x64 px from Spotify CDN
#define MAX_ARTWORK_BYTES   8192      // max JPEG buffer in RAM

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     28
#define TASK_H       38
#define TASK_MARGIN   3
#define BAR_W         3
