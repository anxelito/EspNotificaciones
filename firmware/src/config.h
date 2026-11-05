// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Now Playing
#define NOWPLAYING_POLL_MS  3000UL    // poll Spotify every 3s
#define WEATHER_POLL_MS     600000UL  // outside weather every 10 min
#define WEATHER_RETRY_MS    60000UL   // after a failed weather fetch
#define NOWPLAYING_STOP_POLLS 6       // consecutive not-playing/failed polls (~18 s) before "stopped"
#define TASKS_MAX_FAILS     3         // consecutive failed task fetches before OFFLINE
#define ARTWORK_SIZE        120       // sharp cover inside the composed 320x200 music image
#define MAX_ARTWORK_BYTES   16384     // composed music JPEG buffer (x3: worker, published, UI)

// Network worker (core 0)
#define NET_WORKER_CORE     0
#define NET_WORKER_STACK    16384     // bytes (ESP-IDF stack depth unit = bytes)
#define NET_WORKER_PRIO     1
#define NET_LOOP_DELAY_MS   50
#define HTTP_TIMEOUT_MS     8000      // connect / TLS handshake / read, each (ESP32 TLS can take >3 s)
#define WIFI_RETRY_MS       10000UL   // WiFi.reconnect() cadence while down

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     26

// Pins
#define PIN_DHT        27
#define PIN_TOUCH      32
#define PIN_BOOT_BTN   0      // on-board BOOT button acts like the touch pad; -1 = off
#define BACKLIGHT_PIN  -1     // -1 = no dimming

// Sensor diagnostics on the serial monitor (pio device monitor, 115200):
// every DHT11 read and every change on the touch pin. 0 = quiet.
#define SENSOR_DEBUG   1

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
#define WINDOW_FRAME_MS    150UL    // window (sky/weather) animation
#define PULSE_FRAME_MS     60UL     // urgent task dot pulse
#define EQ_FRAME_MS        90UL     // music equaliser bars
#define TOAST_MS           6000UL   // "Nueva tarea" banner
// Idle behaviour (logic/cat_behavior): random act between rests
#define CAT_REST_MIN_MS    2500UL
#define CAT_REST_MAX_MS    7000UL
#define CAT_WALK_PX_PER_S  28UL
#define CAT_WALK_MIN_PX    30
#define CAT_LOOK_MS        2200UL
#define CAT_GROOM_MS       2600UL
#define CAT_YAWN_MS        1600UL
#define CAT_STRETCH_MS     1800UL
#define CAT_HOP_MS         700UL
