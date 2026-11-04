// firmware/src/main.ino
// Core 1 (loop): UI only. All networking lives in the core-0 worker
// (net/networker.*); loop() reads its published snapshot and never blocks on
// HTTP. Old screens (redrawAll) kept until the UI rewrite.
#include "config.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include "nowplaying.h"
#include "net/networker.h"
#include "ui/theme.h"
#include "ui/chrome.h"
#include "ui/screen_tasks.h"
#include "ui/screen_music.h"
#include <SPIFFS.h>
#include <WiFi.h>

static NetSnapshot snap;                 // latest copy from the worker
static NetSnapshot shown;                // data the screen was last drawn with
static bool        drawnOnce     = false;
static ScreenMode  currentScreen = SCREEN_TASKS;
static char        shownTime[6]  = "";
static unsigned long lastCheck   = 0;

// Local artwork copy (core 1 only; decoded by TJpg on this core)
static uint8_t*    artworkBuf     = nullptr;
static int         artworkLen     = 0;
static uint32_t    artworkVersion = 0;   // songVersion artworkBuf belongs to

static void formatTime(char* out, size_t len) {
    int h, m;
    if (clockGetHM(&h, &m)) snprintf(out, len, "%02d:%02d", h, m);
    else                    strlcpy(out, "--:--", len);
}

static void drawArtworkIfCurrent() {
    if (artworkLen > 0 && artworkVersion == snap.songVersion) {
        renderArtwork(artworkBuf, artworkLen, MUSIC_ART_X, MUSIC_ART_Y);
    }
}

void setup() {
    Serial.begin(115200);

    displayInit();
    drawSplash("Iniciando...");

    SPIFFS.begin(true);  // format on first run if needed
    static TaskItem cached[MAX_TASKS];
    int cachedCount = loadTasks(cached);
    sortTasks(cached, cachedCount);

    artworkBuf = (uint8_t*)malloc(MAX_ARTWORK_BYTES);
    if (!artworkBuf) Serial.println("ERROR: no heap for artwork buffer");

    // Bring up the network stack here (core 1, before the worker exists) so
    // configTime() sets TZ without racing localtime_r() on the other core.
    WiFi.mode(WIFI_STA);
    clockBegin();

    netStart(cached, cachedCount);       // worker connects WiFi on core 0
}

void loop() {
    unsigned long now = millis();
    if ((now - lastCheck) < UI_REFRESH_MS) {
        delay(5);
        return;
    }
    lastCheck = now;

    netGetSnapshot(&snap);

    char t[6];
    formatTime(t, sizeof(t));
    bool timeChanged = strcmp(t, shownTime) != 0;

    if (snap.playing) {
        bool songChanged = (currentScreen != SCREEN_NOW_PLAYING) ||
                           (snap.songVersion != shown.songVersion);
        bool npChanged   = memcmp(&snap.np, &shown.np, sizeof(snap.np)) != 0;

        if (songChanged || timeChanged || !drawnOnce) {
            HeaderInfo hi = {};
            hi.title = "SONANDO";
            hi.online = snap.online;
            hi.timeKnown = clockGetHM(&hi.hour, &hi.minute);
            hi.playing = true;
            static const RoomReading noRoom;   // sensors wired in Task 10
            hi.room = &noRoom;
            hi.roomFresh = false;
            drawHeader(hi);
        }
        if (songChanged || !drawnOnce) {
            bool artOk = artworkBuf && artworkLen > 0 && artworkVersion == snap.songVersion;
            drawMusicScreen(snap.np, artOk ? artworkBuf : nullptr, artOk ? artworkLen : 0);
            drawPageDots(2, 3);
        } else if (npChanged) {
            drawMusicProgress(snap.np);
        }
        if (songChanged || timeChanged || !drawnOnce) {
            currentScreen = SCREEN_NOW_PLAYING;
            strlcpy(shownTime, t, sizeof(shownTime));
            drawnOnce = true;
        }

        // Artwork arrives after the song info; pick it up once its version
        // matches the current song (the worker writes 0 bytes on failure).
        if (artworkBuf && artworkVersion != snap.songVersion) {
            uint32_t v = 0;
            int n = netCopyArtwork(artworkBuf, MAX_ARTWORK_BYTES, &v);
            if (v == snap.songVersion) {
                artworkVersion = v;
                artworkLen     = n;
                drawArtworkIfCurrent();
            }
        }
    } else {
        bool tasksChanged = (snap.online != shown.online) ||
                            (snap.taskCount != shown.taskCount) ||
                            memcmp(snap.tasks, shown.tasks, sizeof(snap.tasks)) != 0;

        if (currentScreen != SCREEN_TASKS || tasksChanged || timeChanged || !drawnOnce) {
            // Nothing playing — (back to) the tasks screen
            currentScreen = SCREEN_TASKS;
            int pending = 0;
            for (int i = 0; i < snap.taskCount; i++)
                if (strcmp(snap.tasks[i].status, "pending") == 0) pending++;
            char title[24];
            snprintf(title, sizeof(title), "TAREAS " TITLE_DOT " %d", pending);
            HeaderInfo hi = {};
            hi.title = title;
            hi.online = snap.online;
            hi.timeKnown = clockGetHM(&hi.hour, &hi.minute);
            hi.playing = false;
            static const RoomReading noRoom;   // sensors wired in Task 10
            hi.room = &noRoom;
            hi.roomFresh = false;
            drawHeader(hi);
            drawTasksScreen(snap.tasks, snap.taskCount);
            drawPageDots(1, snap.playing ? 3 : 2);
            strlcpy(shownTime, t, sizeof(shownTime));
            drawnOnce = true;
        }
    }

    memcpy(&shown, &snap, sizeof(shown));
}
