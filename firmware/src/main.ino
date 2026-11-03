// firmware/src/main.ino
#include "config.h"
#include "secrets.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include "nowplaying.h"
#include <SPIFFS.h>

static TaskItem   tasks[MAX_TASKS];
static int        taskCount     = 0;
static bool       online        = false;
static unsigned long lastPoll   = 0;
static unsigned long lastNTP    = 0;
static unsigned long lastNPPoll = 0;

// Now Playing state
static NowPlaying  nowPlaying;
static ScreenMode  currentScreen   = SCREEN_TASKS;
static char        lastSongKey[128] = "";  // "title|artist" to detect song change
static uint8_t*    artworkBuf      = nullptr;
static int         artworkLen      = 0;

void setup() {
    Serial.begin(115200);

    displayInit();
    drawSplash("Iniciando...");

    SPIFFS.begin(true);  // format on first run if needed
    taskCount = loadTasks(tasks);
    if (taskCount > 0) {
        sortTasks(tasks, taskCount);
        redrawAll(tasks, taskCount, "--:--", false);
    }

    drawSplash("Conectando WiFi...");
    online = connectWiFi();

    if (online) {
        drawSplash("Sincronizando hora...");
        syncNTP();
        lastNTP = millis();
    }

    // Pre-allocate artwork buffer
    artworkBuf = (uint8_t*)malloc(MAX_ARTWORK_BYTES);
    if (!artworkBuf) {
        Serial.println("ERROR: no heap for artwork buffer");
    }

    String t = online ? getCurrentTime() : "--:--";
    redrawAll(tasks, taskCount, t, online);
}

// Build a unique key for the current song to detect changes
static void buildSongKey(const NowPlaying& np, char* out, int maxLen) {
    snprintf(out, maxLen, "%s|%s", np.title, np.artist);
}

void loop() {
    unsigned long now = millis();

    // --- NTP sync (every hour) ---
    if (online && (now - lastNTP >= NTP_SYNC_INTERVAL)) {
        syncNTP();
        lastNTP = now;
    }

    // --- Tasks polling (always, every 5s) ---
    if (now - lastPoll >= POLL_INTERVAL_MS) {
        int result = fetchTasks(tasks);
        if (result >= 0) {
            taskCount = result;
            online    = true;
            sortTasks(tasks, taskCount);
            saveTasks(tasks, taskCount);
        } else {
            online = false;
        }
        // Only redraw tasks screen if that's what's showing
        if (currentScreen == SCREEN_TASKS) {
            String t = online ? getCurrentTime() : "--:--";
            redrawAll(tasks, taskCount, t, online);
        }
        lastPoll = now;
    }

    // --- Now Playing polling (every 3s) ---
    if (online && (now - lastNPPoll >= NOWPLAYING_POLL_MS)) {
        NowPlaying np;
        memset(&np, 0, sizeof(np));

        bool ok = fetchNowPlaying(&np);

        if (ok && np.is_playing) {
            // Check if song changed
            char songKey[128];
            buildSongKey(np, songKey, sizeof(songKey));
            bool songChanged = (strcmp(songKey, lastSongKey) != 0);

            if (currentScreen != SCREEN_NOW_PLAYING || songChanged) {
                // Full redraw: new song or switching from tasks
                nowPlaying = np;
                String t = getCurrentTime();
                drawNowPlaying(nowPlaying, t);

                // Download and render artwork (only if song changed)
                if (songChanged && artworkBuf && strlen(np.artwork_url) > 0) {
                    artworkLen = fetchArtwork(np.artwork_url, artworkBuf, MAX_ARTWORK_BYTES);
                    if (artworkLen > 0) {
                        renderArtwork(artworkBuf, artworkLen, 20, HEADER_H + 30);
                    }
                    strlcpy(lastSongKey, songKey, sizeof(lastSongKey));
                } else if (!songChanged && artworkLen > 0) {
                    // Same song, re-render cached artwork (screen was just drawn)
                    renderArtwork(artworkBuf, artworkLen, 20, HEADER_H + 30);
                }

                currentScreen = SCREEN_NOW_PLAYING;
            } else {
                // Same song, just update progress bar region
                nowPlaying.progress_ms = np.progress_ms;
                nowPlaying.duration_ms = np.duration_ms;
                // Redraw only the progress section (lightweight)
                String t = getCurrentTime();
                drawNowPlaying(nowPlaying, t);
                if (artworkLen > 0) {
                    renderArtwork(artworkBuf, artworkLen, 20, HEADER_H + 30);
                }
            }
        } else if (currentScreen == SCREEN_NOW_PLAYING) {
            // Nothing playing — switch back to tasks
            currentScreen = SCREEN_TASKS;
            lastSongKey[0] = '\0';
            String t = online ? getCurrentTime() : "--:--";
            redrawAll(tasks, taskCount, t, online);
        }

        lastNPPoll = now;
    }
}
