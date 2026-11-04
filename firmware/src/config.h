// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Now Playing
#define NOWPLAYING_POLL_MS  3000UL    // poll Spotify every 3s
#define NOWPLAYING_MAX_FAILS 3        // consecutive failed polls before "not playing"
#define ARTWORK_SIZE        64        // 64x64 px from Spotify CDN
#define MAX_ARTWORK_BYTES   8192      // max JPEG buffer in RAM

// Network worker (core 0)
#define NET_WORKER_CORE     0
#define NET_WORKER_STACK    16384     // bytes (ESP-IDF stack depth unit = bytes)
#define NET_WORKER_PRIO     1
#define NET_LOOP_DELAY_MS   50
#define HTTP_TIMEOUT_MS     3000      // connect / TLS handshake / read, each
#define WIFI_RETRY_MS       10000UL   // WiFi.reconnect() cadence while down

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     26

// Pins
#define PIN_DHT        27
#define PIN_TOUCH      32
#define BACKLIGHT_PIN  -1     // -1 = no dimming

// Room comfort thresholds
#define TEMP_COLD_BELOW  18.0f
#define TEMP_HOT_ABOVE   25.0f
#define HUM_DRY_BELOW    30.0f
#define HUM_HUMID_ABOVE  65.0f

// Sleep window (local hours)
#define SLEEP_START_H  23
#define SLEEP_END_H    8

// Timings
#define DHT_READ_MS        2000UL
#define SENSOR_STALE_MS    30000UL
#define TOUCH_DEBOUNCE_MS  30UL
#define LONG_PRESS_MS      1000UL
#define IDLE_RETURN_MS     30000UL
#define NIGHT_WAKE_MS      10000UL
#define PET_MS             2000UL
#define BLINK_EVERY_MS     4000UL
#define BLINK_LEN_MS       150UL
#define CAT_FRAME_MS       80UL     // cat redraw cadence on Habitacion (~12 fps)
