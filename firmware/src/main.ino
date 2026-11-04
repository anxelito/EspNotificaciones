// firmware/src/main.ino
// Core 1 (loop): touch, DHT11, cat/screen state and all drawing. All
// networking lives in the core-0 worker (net/networker.*); loop() only reads
// its published snapshot and never blocks on HTTP.
#include "config.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include "nowplaying.h"
#include "net/networker.h"
#include "logic/room.h"
#include "logic/gesture.h"
#include "logic/ui_state.h"
#include "logic/cat_state.h"
#include "hw/sensors.h"
#include "hw/touch.h"
#include "hw/backlight.h"
#include "ui/theme.h"
#include "ui/chrome.h"
#include "ui/screen_tasks.h"
#include "ui/screen_music.h"
#include "ui/screen_room.h"
#include "ui/cat_draw.h"
#include <SPIFFS.h>
#include <WiFi.h>

#define LOOP_IDLE_MS     2      // per-iteration yield (touch polled every pass)
#define BL_AWAKE         255
#define BL_ASLEEP        40

// --- live state -------------------------------------------------------------
static RoomReading     room;
static GestureDetector gesture;
static UiState         ui;
static CatState        cat;
static NetSnapshot     snap;             // latest copy from the worker

// --- what is currently on the glass -----------------------------------------
// Everything the header shows; redraw it only when this changes.
struct HeaderKey {
    char    title[24];
    bool    online, timeKnown, playing, haveRoom;
    int     hour, minute, temp, hum;
    Comfort comfort;                     // temp colour can change without int change
};

static bool       drawnOnce       = false;
static Screen     drawnScreen     = Screen::ROOM;
static int        drawnDotsCount  = 0;
static HeaderKey  drawnHeader;
static uint32_t   lastCatFrame    = 0;
static int        drawnBacklight  = -1;
static TaskItem   drawnTasks[MAX_TASKS];
static int        drawnTaskCount  = -1;
static NowPlaying drawnNp;
static uint32_t   drawnSongVer    = 0;

// Local artwork copy (core 1 only; decoded by TJpg on this core)
static uint8_t*   artworkBuf      = nullptr;
static int        artworkLen      = 0;
static uint32_t   artworkVersion  = 0;   // songVersion artworkBuf belongs to

static int pendingCount() {
    int n = 0;
    for (int i = 0; i < snap.taskCount; i++)
        if (strcmp(snap.tasks[i].status, "pending") == 0) n++;
    return n;
}

static void buildHeaderKey(HeaderKey& k, bool timeKnown, int hour, int minute, bool fresh) {
    memset(&k, 0, sizeof(k));
    switch (ui.screen) {
        case Screen::ROOM:  strlcpy(k.title, "HABITACION", sizeof(k.title)); break;
        case Screen::TASKS:
            snprintf(k.title, sizeof(k.title), "TAREAS " TITLE_DOT " %d", pendingCount());
            break;
        case Screen::MUSIC: strlcpy(k.title, "SONANDO", sizeof(k.title)); break;
    }
    k.online    = snap.online;
    k.timeKnown = timeKnown;
    k.hour      = timeKnown ? hour : 0;
    k.minute    = timeKnown ? minute : 0;
    k.playing   = snap.playing;
    k.haveRoom  = room.everOk && fresh;
    if (k.haveRoom) {
        k.temp    = (int)lroundf(room.tempC);
        k.hum     = (int)lroundf(room.humidity);
        k.comfort = classifyTemp(room.tempC);
    }
}

static bool artworkReady() {
    return artworkBuf && artworkLen > 0 && artworkVersion == snap.songVersion;
}

// Copy the artwork once per song, only after the worker has published the
// artwork for the current songVersion (cheap peek first, no 8 KB copy).
static bool pickUpArtwork() {
    if (!artworkBuf || artworkVersion == snap.songVersion) return false;
    if (netArtworkVersion() != snap.songVersion) return false;
    uint32_t v = 0;
    int n = netCopyArtwork(artworkBuf, MAX_ARTWORK_BYTES, &v);
    if (v != snap.songVersion) return false;   // raced with a newer song
    artworkVersion = v;
    artworkLen     = n;
    return n > 0;
}

static void drawCatFrame(uint32_t now) {
    CatPose p;
    p.mood      = cat.mood;
    p.blink     = catBlinking(cat, now);
    p.humid     = roomIsFresh(room, now) ? classifyHumidity(room.humidity) : HumidMood::UNKNOWN;
    uint16_t body = comfortColor(roomIsFresh(room, now) ? classifyTemp(room.tempC)
                                                         : Comfort::UNKNOWN);
    p.body      = body;                  // catDraw darkens when SLEEPING
    p.nowMs     = now;
    p.moodSince = cat.since;
    catDraw(p, ROOM_CAT_X, ROOM_CAT_Y);
}

static void render(uint32_t now, bool timeKnown, int hour, int minute, bool newArt) {
    const bool fresh       = roomIsFresh(room, now);
    const bool screenNew   = !drawnOnce || ui.screen != drawnScreen;
    const int  dotsCount   = uiScreenCount(snap.playing);

    // Header: on screen change or when anything it shows changed.
    HeaderKey hk;
    buildHeaderKey(hk, timeKnown, hour, minute, fresh);
    if (screenNew || memcmp(&hk, &drawnHeader, sizeof(hk)) != 0) {
        HeaderInfo hi = {};
        hi.title     = hk.title;
        hi.online    = snap.online;
        hi.timeKnown = timeKnown;
        hi.hour      = hour;
        hi.minute    = minute;
        hi.playing   = snap.playing;
        hi.room      = &room;
        hi.roomFresh = fresh;
        drawHeader(hi);
        drawnHeader = hk;
    }

    // Body.
    switch (ui.screen) {
        case Screen::ROOM:
            if (screenNew) {
                drawRoomScreen();                      // clears body, invalidates strip
                drawRoomStrip(room, fresh);
                drawCatFrame(now);
                lastCatFrame = now;
            } else if ((now - lastCatFrame) >= CAT_FRAME_MS) {
                drawRoomStrip(room, fresh);            // repaints changed columns only
                drawCatFrame(now);
                lastCatFrame = now;
            }
            break;

        case Screen::TASKS:
            if (screenNew || snap.taskCount != drawnTaskCount ||
                memcmp(snap.tasks, drawnTasks, sizeof(drawnTasks)) != 0) {
                drawTasksScreen(snap.tasks, snap.taskCount);
                memcpy(drawnTasks, snap.tasks, sizeof(drawnTasks));
                drawnTaskCount = snap.taskCount;
            }
            break;

        case Screen::MUSIC:
            if (screenNew || snap.songVersion != drawnSongVer) {
                bool ok = artworkReady();
                drawMusicScreen(snap.np, ok ? artworkBuf : nullptr, ok ? artworkLen : 0);
                drawnNp      = snap.np;
                drawnSongVer = snap.songVersion;
            } else {
                if (newArt && artworkReady())          // arrived after the text
                    renderArtwork(artworkBuf, artworkLen, MUSIC_ART_X, MUSIC_ART_Y);
                if (memcmp(&snap.np, &drawnNp, sizeof(drawnNp)) != 0) {
                    drawMusicProgress(snap.np);
                    drawnNp = snap.np;
                }
            }
            break;
    }

    // Page dots: on screen change, or when Spotify joins/leaves the cycle.
    if (screenNew || dotsCount != drawnDotsCount) {
        drawPageDots(uiScreenIndex(ui.screen), dotsCount);
        drawnDotsCount = dotsCount;
    }

    drawnScreen = ui.screen;
    drawnOnce   = true;
}

void setup() {
    Serial.begin(115200);

    displayInit();
    // Cat sprite (~18 KB) first, before the worker and artwork buffers.
    uint32_t heapBefore = ESP.getFreeHeap();
    bool sprOk = catDrawInit();
    Serial.printf("[boot] cat sprite %s, heap %u -> %u B free (expect >= 40 KB)\n",
                  sprOk ? "OK" : "FAILED", (unsigned)heapBefore,
                  (unsigned)ESP.getFreeHeap());
    drawSplash("Iniciando...");

    sensorsInit();
    touchInit();
    backlightInit();

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

    uint32_t now = millis();
    ui.screen      = Screen::ROOM;       // home
    ui.lastTouchMs = now;
    cat.since      = now;
    Serial.printf("[boot] ready, heap %u B free\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
    uint32_t now = millis();

    sensorsPoll(room, now);              // self-throttled to DHT_READ_MS

    // Touch every pass. snap.playing is from the previous pass (<= a few ms old).
    switch (gesture.update(touchPressed(), now)) {
        case Gesture::TAP:
            if (uiOnTap(ui, snap.playing, catIsAsleep(cat), now) == TapEffect::WOKE_CAT)
                catOnWakeTap(cat, now);
            break;
        case Gesture::LONG_PRESS:
            ui.lastTouchMs = now;        // any touch counts as activity
            if (ui.screen == Screen::ROOM) catOnPet(cat, now);
            break;
        default:
            break;
    }

    netGetSnapshot(&snap);

    int hour = 0, minute = 0;
    bool timeKnown = clockGetHM(&hour, &minute);

    catUpdate(cat, isSleepHour(timeKnown ? hour : -1), now);
    uiTick(ui, snap.playing, now);       // idle timeout / music ended -> ROOM

    int bl = catIsAsleep(cat) ? BL_ASLEEP : BL_AWAKE;
    if (bl != drawnBacklight) {
        backlightSet((uint8_t)bl);
        drawnBacklight = bl;
    }

    bool newArt = pickUpArtwork();
    render(now, timeKnown, hour, minute, newArt);

    delay(LOOP_IDLE_MS);
}
