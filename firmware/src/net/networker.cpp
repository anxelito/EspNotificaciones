// firmware/src/net/networker.cpp
#include "networker.h"
#include "../network.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// ---------------------------------------------------------------------------
// Shared state — read/written ONLY while holding s_mutex, and only with plain
// copies (memcpy). No HTTP / SPIFFS / Serial while the mutex is held.
// ---------------------------------------------------------------------------
static SemaphoreHandle_t s_mutex = nullptr;
static NetSnapshot       s_snap;                       // published snapshot
static uint8_t           s_art[MAX_ARTWORK_BYTES];     // published artwork
static int               s_artLen     = 0;
static uint32_t          s_artVersion = 0;             // songVersion of s_art

// ---------------------------------------------------------------------------
// Worker-private state — touched only by the worker task (core 0). Static so
// the large buffers do not live on the worker stack.
// ---------------------------------------------------------------------------
static NetSnapshot w_snap;                              // working copy
static TaskItem    w_fetch[MAX_TASKS];
static NowPlaying  w_np;
static char        w_songKey[sizeof(w_np.title) + sizeof(w_np.artist) + 2];
static uint8_t     w_dl[MAX_ARTWORK_BYTES];             // download buffer
static int         w_taskFails = 0;                     // consecutive task fetch failures
static int         w_npFails = 0;                       // consecutive NP poll failures

static void publishSnapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(&s_snap, &w_snap, sizeof(s_snap));
    xSemaphoreGive(s_mutex);
}

static void publishArtwork(int len, uint32_t version) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (len > 0) memcpy(s_art, w_dl, len);
    s_artLen     = len > 0 ? len : 0;
    s_artVersion = version;
    xSemaphoreGive(s_mutex);
}

// Returns true if the published snapshot changed.
static bool pollTasks() {
    memset(w_fetch, 0, sizeof(w_fetch));       // zeroed so memcmp is meaningful
    int n = fetchTasks(w_fetch);
    if (n < 0) {
        if (w_taskFails < TASKS_MAX_FAILS) w_taskFails++;
        if (w_taskFails < TASKS_MAX_FAILS) return false;   // transient: stay online
        if (!w_snap.online) return false;
        w_snap.online = false;                 // keep last good tasks (cache)
        return true;
    }
    w_taskFails = 0;
    sortTasks(w_fetch, n);
    bool changed = (n != w_snap.taskCount) ||
                   memcmp(w_fetch, w_snap.tasks, sizeof(w_fetch)) != 0;
    if (changed) {
        // Only write flash when the list actually changed (avoids SPIFFS wear
        // from rewriting the same file every POLL_INTERVAL_MS).
        if (!saveTasks(w_fetch, n)) Serial.println("[net] saveTasks failed");
        memcpy(w_snap.tasks, w_fetch, sizeof(w_fetch));
        w_snap.taskCount = n;
    }
    bool wasOnline = w_snap.online;
    w_snap.online = true;
    return changed || !wasOnline;
}

// Returns true if a new song started (artwork must be downloaded).
static bool pollNowPlaying() {
    memset(&w_np, 0, sizeof(w_np));
    bool ok = fetchNowPlaying(&w_np);
    if (ok && w_np.is_playing) {
        w_npFails = 0;
        char key[sizeof(w_songKey)];
        snprintf(key, sizeof(key), "%s|%s", w_np.title, w_np.artist);
        bool songChanged = strcmp(key, w_songKey) != 0;
        if (songChanged) {
            strlcpy(w_songKey, key, sizeof(w_songKey));
            w_snap.songVersion++;
        }
        w_snap.playing = true;
        w_snap.np      = w_np;
        return songChanged;
    }
    // Not playing, or a failed poll: keep the last state for a few polls.
    // Failures are often transient, and Last.fm briefly drops "now playing"
    // between tracks; only NOWPLAYING_STOP_POLLS in a row count as stopped.
    if (w_npFails < NOWPLAYING_STOP_POLLS) w_npFails++;
    if (w_snap.playing && w_npFails < NOWPLAYING_STOP_POLLS) return false;
    // Confirmed not playing (or too many failures): forget the song so the next
    // play — even of the same track — counts as a change and refreshes artwork.
    w_snap.playing = false;
    w_songKey[0]   = '\0';
    return false;
}

static void downloadArtwork() {
    uint32_t version = w_snap.songVersion;
    int len = -1;
    if (w_snap.np.artwork_url[0] != '\0') {
        len = fetchArtwork(w_snap.np.artwork_url, w_dl, MAX_ARTWORK_BYTES);
    }
    publishArtwork(len, version);              // len<=0 → "no artwork" for it
}

static void workerTask(void*) {
    connectWiFi();                             // blocks core 0 only (≤10 s)

    uint32_t lastTasks = 0, lastNP = 0, lastRetry = millis(), lastStackLog = 0;
    bool wasUp = false;

    for (;;) {
        uint32_t now = millis();

        if (WiFi.status() != WL_CONNECTED) {
            if (wasUp || w_snap.online || w_snap.playing) {
                w_snap.online  = false;
                w_snap.playing = false;
                w_songKey[0]   = '\0';
                w_npFails      = 0;
                w_taskFails    = 0;
                publishSnapshot();
            }
            wasUp = false;
            if ((now - lastRetry) >= WIFI_RETRY_MS) {
                Serial.println("[net] WiFi down, reconnecting");
                WiFi.reconnect();              // async; status polled above
                lastRetry = now;
            }
            vTaskDelay(pdMS_TO_TICKS(NET_LOOP_DELAY_MS));
            continue;
        }

        bool forceNow = !wasUp;                // (re)connected: poll at once
        wasUp = true;

        if (forceNow || (now - lastTasks) >= POLL_INTERVAL_MS) {
            lastTasks = now;
            if (pollTasks()) publishSnapshot();
        }

        if (forceNow || (now - lastNP) >= NOWPLAYING_POLL_MS) {
            lastNP = now;
            bool newSong = pollNowPlaying();
            publishSnapshot();                 // progress changes every poll
            if (newSong) downloadArtwork();    // after publish: UI shows text first
        }

        if ((now - lastStackLog) >= 60000UL) {
            lastStackLog = now;
            Serial.printf("[net] stack free min %u B, heap free %u B\n",
                          (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                          (unsigned)ESP.getFreeHeap());
        }

        vTaskDelay(pdMS_TO_TICKS(NET_LOOP_DELAY_MS));
    }
}

void netStart(const TaskItem* cached, int cachedCount) {
    if (s_mutex) return;                       // already started

    memset(&w_snap, 0, sizeof(w_snap));
    if (cached && cachedCount > 0) {
        if (cachedCount > MAX_TASKS) cachedCount = MAX_TASKS;
        memcpy(w_snap.tasks, cached, cachedCount * sizeof(TaskItem));
        w_snap.taskCount = cachedCount;
        sortTasks(w_snap.tasks, w_snap.taskCount);
    }
    memcpy(&s_snap, &w_snap, sizeof(s_snap));  // before the worker exists

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        Serial.println("[net] ERROR: mutex alloc failed");
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(workerTask, "net", NET_WORKER_STACK,
                                            nullptr, NET_WORKER_PRIO, nullptr,
                                            NET_WORKER_CORE);
    if (ok != pdPASS) Serial.println("[net] ERROR: worker task create failed");
}

void netGetSnapshot(NetSnapshot* out) {
    if (!out) return;
    if (!s_mutex) {                            // netStart not called / failed
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(out, &s_snap, sizeof(*out));
    xSemaphoreGive(s_mutex);
}

int netCopyArtwork(uint8_t* dst, int maxLen, uint32_t* version) {
    if (!s_mutex) {
        if (version) *version = 0;
        return 0;
    }
    int n = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (version) *version = s_artVersion;
    if (dst && s_artLen > 0 && s_artLen <= maxLen) {
        memcpy(dst, s_art, s_artLen);
        n = s_artLen;
    }
    xSemaphoreGive(s_mutex);
    return n;
}

uint32_t netArtworkVersion() {
    if (!s_mutex) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t v = s_artVersion;
    xSemaphoreGive(s_mutex);
    return v;
}
