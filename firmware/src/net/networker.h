// firmware/src/net/networker.h
// Network worker: a FreeRTOS task pinned to core 0 owns WiFi and every HTTPS
// request. loop() (core 1) only reads published copies, so it never blocks on
// the network. Never call TFT functions from the worker.
#pragma once
#include <Arduino.h>
#include "../config.h"
#include "../tasks.h"
#include "../nowplaying.h"
#include "../weather.h"

struct NetSnapshot {
    bool       online;       // WiFi up AND last task fetch succeeded
    int        taskCount;
    TaskItem   tasks[MAX_TASKS]; // sorted; last good list (cached when offline)
    bool       playing;      // last Spotify poll succeeded and is_playing
    NowPlaying np;
    uint32_t   songVersion;  // ++ on song change (title|artist)
    Weather    weather;      // last good outside weather (ok=false until first)
};

// Creates the mutex and starts the worker pinned to core 0. `cached` (may be
// null) seeds the task list shown until the first successful fetch.
void netStart(const TaskItem* cached, int cachedCount);

// Copies the latest snapshot (memcpy under mutex; never blocks on HTTP).
void netGetSnapshot(NetSnapshot* out);

// Copies the current artwork JPEG into dst. Writes the songVersion the artwork
// belongs to into *version (even when returning 0). Returns bytes copied, or 0
// if no artwork is available (none downloaded / download failed / too big).
int netCopyArtwork(uint8_t* dst, int maxLen, uint32_t* version);

// Cheap peek: songVersion of the currently published artwork (no copy). Lets
// loop() call netCopyArtwork only once the artwork for the current song exists.
uint32_t netArtworkVersion();
