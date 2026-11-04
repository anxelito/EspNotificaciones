// firmware/src/network.h
#pragma once
#include <Arduino.h>
#include "tasks.h"
#include "nowplaying.h"

// Blocks up to ~10 s. Returns true if connected. Call only from the net worker.
bool connectWiFi();

// Starts ESP SNTP: configTime(TZ_OFFSET*3600, 0, NTP_SERVER). Call once from
// setup() after WiFi.mode(WIFI_STA). SNTP re-syncs by itself; time reads are
// thread-safe.
void clockBegin();

// Local hour/minute. Returns false if the clock was never synced.
bool clockGetHM(int* hour, int* minute);

// All fetch* functions do blocking HTTPS (timeouts <= 3000 ms each step):
// call them only from the net worker on core 0, never from loop().

// Fetches tasks from API. Returns task count, -1 on error.
int fetchTasks(TaskItem* tasks);

// Fetches now-playing info from backend. Returns true on success.
bool fetchNowPlaying(NowPlaying* np);

// Downloads JPEG artwork into buffer. Returns bytes written, -1 on error.
int fetchArtwork(const char* url, uint8_t* buf, int maxLen);
