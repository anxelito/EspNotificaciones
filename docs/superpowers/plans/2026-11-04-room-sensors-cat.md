# Room Sensors, Touch & Cat Mascot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a DHT11 room sensor, TTP223 touch navigation, a restyled UI ("tarjetas suaves") and an animated cat on a new home screen to the ESP32 task panel.

**Architecture:** All decision logic (comfort bands, sleep window, gesture detection, screen state machine, cat mood) lives in pure C++ under `firmware/src/logic/` with no Arduino includes, unit-tested on the host with PlatformIO `native` + Unity. Networking (tasks, Spotify, artwork) moves to a FreeRTOS task pinned to core 0 that publishes a mutex-guarded snapshot; the Arduino `loop()` on core 1 owns the TFT, touch and DHT11 and never blocks on HTTP. Rendering is split into `firmware/src/ui/` modules; the cat is drawn into a `TFT_eSprite` and pushed as one block to avoid flicker.

**Tech Stack:** ESP32 Arduino (PlatformIO `espressif32`), TFT_eSPI (ST7789 320×240), TJpg_Decoder, ArduinoJson 7, `adafruit/DHT sensor library`, FreeRTOS, Unity (native tests).

**Spec:** `docs/superpowers/specs/2026-11-04-room-sensors-cat-design.md`

## Global Constraints

- Pins: DHT11 data **GPIO 27**, TTP223 out **GPIO 32**, `BACKLIGHT_PIN -1` default (no dimming). TFT pins 18/19/23/15/2/4 untouched.
- Temp bands: `< 18 °C` blue `#5ab0ff` "fresco"; `18–25 °C` green `#4fd18b` "bien"; `> 25 °C` orange `#ff8a3d` "calor". Unknown temp → green, text `--°`.
- Humidity faces: `> 65 %` sweaty, `< 30 %` dry. Unknown → `--%`.
- Sleep window 23:00–08:00 local; time never synced → awake.
- Long press `> 1000 ms`; inactivity return to Habitación `30000 ms`; night wake `10000 ms`; pet animation `2000 ms`.
- DHT11 read every `2000 ms`; values go stale (`--`) after `30000 ms` without a good read.
- Screen order Habitación → Tareas → Spotify (Spotify only while playing). Spotify never auto-switches; ♫ in header while playing.
- Style: bg `#0b0d12`, card `#161a24`, radius 8, side margin 8, header ~26 px, bottom strip ~56 px on Habitación, page dots at bottom. UI text in Spanish.
- Offline: header time replaced by `OFFLINE` in red. Pending count in title `TAREAS · N`.
- All thresholds/timings/pins as `#define` in `firmware/src/config.h`. `config.h` must stay free of Arduino includes (logic code includes it).
- All time comparisons use unsigned subtraction `(now - start) >= dur` (millis wrap safe).
- Only core 1 (`loop()`) touches `tft`. Core 0 never calls TFT functions.

## Review Focus

1. **DHT11 intermittent NaN reads** (common on DHT11) → keep last good value, never flash `--` until 30 s stale. Test in Task 1 (`test_room_nan_keeps_last`).
2. **Song stops while Spotify screen is showing** → UI must leave Spotify (to Habitación), not show a frozen song. Test in Task 3 (`test_ui_music_ends_leaves_music`).
3. **Sleep window crossing midnight & unsynced clock** → 23:59 and 00:30 asleep, 08:00 awake, hour `-1` awake. Test in Task 1 (`test_sleep_window_wrap`, `test_sleep_unknown_time`).
4. **`millis()` wraparound** during a press or timeout → gesture and timeouts still correct. Tests in Task 2 (`test_gesture_millis_wrap`) and Task 3 (`test_ui_timeout_millis_wrap`).
5. **Long press must not also produce a tap on release** → exactly one LONG_PRESS, zero TAP. Test in Task 2 (`test_long_press_no_tap_on_release`).

---

### Task 0: Baseline — verify uncommitted Spotify work on hardware

**Files:** none new (existing uncommitted changes in `backend/`, `firmware/`).

- [ ] **Step 1:** Build: `cd firmware && pio run -e esp32dev`. Expected: `SUCCESS`.
- [ ] **Step 2:** Owner flashes (`pio run -e esp32dev -t upload`), plays a song on Spotify. Expected: Now Playing screen with title/artist/artwork/progress; stopping returns to tasks. **This step needs the owner and the board — ask them.**
- [ ] **Step 3:** Fix anything broken (use superpowers:systematic-debugging).
- [ ] **Step 4: Commit** (on a feature branch, not `master`):
```bash
git checkout -b feat/room-cat
git add backend firmware docs/superpowers
git commit -m "feat: spotify now playing screen"
```

### Task 1: Native test env + room logic

**Files:**
- Modify: `firmware/platformio.ini` (add `[env:native]`, add DHT lib to esp32dev)
- Modify: `firmware/src/config.h` (all constants from Global Constraints)
- Create: `firmware/src/logic/room.h`, `firmware/src/logic/room.cpp`
- Test: `firmware/test/test_logic/test_room.cpp` + `firmware/test/test_logic/test_main.cpp`

**Interfaces — Produces:**
```cpp
enum class Comfort   { UNKNOWN, COLD, OK, HOT };
enum class HumidMood { UNKNOWN, NORMAL, DRY, HUMID };
Comfort   classifyTemp(float c);            // NaN → UNKNOWN
HumidMood classifyHumidity(float pct);      // NaN → UNKNOWN
bool      isSleepHour(int hour);            // hour < 0 (unsynced) → false; uses SLEEP_START_H / SLEEP_END_H
struct RoomReading { float tempC = NAN; float humidity = NAN; uint32_t lastOkMs = 0; bool everOk = false; };
void roomAccept(RoomReading& r, float t, float h, uint32_t nowMs); // NaN in either → ignored
bool roomIsFresh(const RoomReading& r, uint32_t nowMs);           // everOk && (now - lastOkMs) < SENSOR_STALE_MS
```

- [ ] **Step 1:** Add to `platformio.ini`:
```ini
[env:native]
platform = native
test_framework = unity
build_src_filter = -<*> +<logic/>
test_build_src = yes
```
and `adafruit/DHT sensor library` + `adafruit/Adafruit Unified Sensor` to esp32dev `lib_deps`. Native needs a host gcc: if `gcc --version` fails, install one (`winget install BrechtSanders.WinLibs.POSIX.UCRT`) and reopen the shell. Fallback when no host compiler: run the same tests on the board with `pio test -e esp32dev` (board plugged).
- [ ] **Step 2:** Add to `config.h`: `PIN_DHT 27`, `PIN_TOUCH 32`, `BACKLIGHT_PIN -1`, `TEMP_COLD_BELOW 18.0f`, `TEMP_HOT_ABOVE 25.0f`, `HUM_DRY_BELOW 30.0f`, `HUM_HUMID_ABOVE 65.0f`, `SLEEP_START_H 23`, `SLEEP_END_H 8`, `DHT_READ_MS 2000UL`, `SENSOR_STALE_MS 30000UL`, `TOUCH_DEBOUNCE_MS 30UL`, `LONG_PRESS_MS 1000UL`, `IDLE_RETURN_MS 30000UL`, `NIGHT_WAKE_MS 10000UL`, `PET_MS 2000UL`, `BLINK_EVERY_MS 4000UL`, `BLINK_LEN_MS 150UL`.
- [ ] **Step 3: Write failing tests** in `test_room.cpp`:
```cpp
TEST classifyTemp: 17.9→COLD, 18.0→OK, 25.0→OK, 25.1→HOT, NAN→UNKNOWN
TEST classifyHumidity: 29.9→DRY, 30→NORMAL, 65→NORMAL, 65.1→HUMID, NAN→UNKNOWN
test_sleep_window_wrap: isSleepHour(22)=false, 23=true, 0=true, 7=true, 8=false, 12=false
test_sleep_unknown_time: isSleepHour(-1)=false
test_room_nan_keeps_last: accept(21,50,t=1000); accept(NAN,NAN,t=3000) → tempC==21, humidity==50, lastOkMs==1000, roomIsFresh(r,20000)==true
test_room_goes_stale: accept(21,50,1000); roomIsFresh(r,1000+30000)==false
test_room_never_ok: fresh RoomReading → roomIsFresh(r,0)==false
```
`test_main.cpp` holds `main()` with `UNITY_BEGIN()`, `RUN_TEST(...)` for every test across files, `UNITY_END()`.
- [ ] **Step 4:** `pio test -e native` → FAIL (undefined symbols).
- [ ] **Step 5:** Implement `room.cpp` (include `<cmath>`, `<cstdint>`, `"../config.h"` only).
- [ ] **Step 6:** `pio test -e native` → all PASS. `pio run -e esp32dev` → SUCCESS.
- [ ] **Step 7: Commit** `feat: room comfort logic + native tests`.

### Task 2: Touch gesture detector

**Files:** Create `firmware/src/logic/gesture.h/.cpp`; Test `firmware/test/test_logic/test_gesture.cpp` (register in `test_main.cpp`).

**Interfaces — Produces:**
```cpp
enum class Gesture { NONE, TAP, LONG_PRESS };
class GestureDetector {
 public:
  Gesture update(bool pressed, uint32_t nowMs); // call every loop
};
```
Semantics: press shorter than `TOUCH_DEBOUNCE_MS` ignored. Release after debounce and before `LONG_PRESS_MS` → `TAP` on the release call. Held ≥ `LONG_PRESS_MS` → `LONG_PRESS` once, while still held; release afterwards → `NONE`.

- [ ] **Step 1: Write failing tests:**
```cpp
test_tap: update(1,0)=NONE; update(1,100)=NONE; update(0,150)=TAP
test_bounce_ignored: update(1,0); update(0,10)=NONE
test_long_press_fires_while_held: update(1,0); update(1,999)=NONE; update(1,1000)=LONG_PRESS; update(1,1500)=NONE
test_long_press_no_tap_on_release: ...after LONG_PRESS, update(0,1600)=NONE
test_gesture_millis_wrap: update(1,0xFFFFFF00); update(0,0x00000040)=TAP   // 0x140 ms elapsed
test_gesture_millis_wrap_long: update(1,0xFFFFFF00); update(1,0x000002F0)=LONG_PRESS
```
- [ ] **Step 2:** `pio test -e native` → FAIL.
- [ ] **Step 3:** Implement (state: pressed flag, press start ms, longFired flag).
- [ ] **Step 4:** `pio test -e native` → PASS.
- [ ] **Step 5: Commit** `feat: touch gesture detector`.

### Task 3: Screen state machine

**Files:** Create `firmware/src/logic/ui_state.h/.cpp`; Test `firmware/test/test_logic/test_ui_state.cpp`.

**Interfaces — Produces:**
```cpp
enum class Screen { ROOM, TASKS, MUSIC };
enum class TapEffect { CHANGED_SCREEN, WOKE_CAT };
struct UiState { Screen screen = Screen::ROOM; uint32_t lastTouchMs = 0; };
TapEffect uiOnTap(UiState& s, bool musicPlaying, bool catAsleep, uint32_t nowMs);
bool      uiTick(UiState& s, bool musicPlaying, uint32_t nowMs); // true if screen changed
int       uiScreenCount(bool musicPlaying);                        // 3 or 2
int       uiScreenIndex(Screen s);                                 // ROOM 0, TASKS 1, MUSIC 2
```
Rules: tap always updates `lastTouchMs`. On ROOM with `catAsleep` → `WOKE_CAT`, screen unchanged. Otherwise advance ROOM→TASKS→MUSIC (if playing)→ROOM. `uiTick`: if screen ≠ ROOM and `now - lastTouchMs >= IDLE_RETURN_MS` → ROOM; if screen == MUSIC and `!musicPlaying` → ROOM.

- [ ] **Step 1: Write failing tests:**
```cpp
test_ui_cycle_with_music: ROOM→TASKS→MUSIC→ROOM
test_ui_cycle_without_music: ROOM→TASKS→ROOM
test_ui_wake_only_on_room: ROOM,asleep → WOKE_CAT, screen ROOM; TASKS,asleep → CHANGED_SCREEN, ROOM
test_ui_timeout_returns_room: tap at 1000 (→TASKS); uiTick(29999+1000)=false; uiTick(31000)=true, screen ROOM
test_ui_timeout_millis_wrap: uint32_t s=0xFFFFF000; tap at s (→TASKS); uiTick(s+29999)=false; uiTick(s+30000)=true  // s+30000 wraps past 0
test_ui_music_ends_leaves_music: screen MUSIC, uiTick(playing=false, now=lastTouch+10) → true, ROOM
test_ui_screen_count: uiScreenCount(true)=3, false=2
```
- [ ] **Step 2:** `pio test -e native` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `pio test -e native` → PASS.
- [ ] **Step 5: Commit** `feat: screen navigation state machine`.

### Task 4: Cat mood machine

**Files:** Create `firmware/src/logic/cat_state.h/.cpp`; Test `firmware/test/test_logic/test_cat_state.cpp`.

**Interfaces — Produces:**
```cpp
enum class CatMood { IDLE, SLEEPING, WOKEN, PETTED };
struct CatState { CatMood mood = CatMood::IDLE; uint32_t since = 0; uint32_t wakeUntil = 0; };
void catUpdate(CatState& c, bool sleepHour, uint32_t nowMs);
void catOnWakeTap(CatState& c, uint32_t nowMs);   // SLEEPING → WOKEN for NIGHT_WAKE_MS
void catOnPet(CatState& c, uint32_t nowMs);       // any → PETTED for PET_MS
bool catIsAsleep(const CatState& c);              // mood == SLEEPING
bool catBlinking(const CatState& c, uint32_t nowMs); // awake moods: true for BLINK_LEN_MS every BLINK_EVERY_MS
```
Rules for `catUpdate`: PETTED ends after `PET_MS` → WOKEN (if `sleepHour`, wake window = `NIGHT_WAKE_MS` from pet start) or IDLE. WOKEN ends after its window → SLEEPING if `sleepHour` else IDLE. IDLE + `sleepHour` → SLEEPING. SLEEPING + `!sleepHour` → IDLE.

- [ ] **Step 1: Write failing tests:**
```cpp
test_cat_falls_asleep: IDLE, catUpdate(sleep=true) → SLEEPING
test_cat_wakes_morning: SLEEPING, catUpdate(sleep=false) → IDLE
test_cat_night_tap_window: SLEEPING; catOnWakeTap(t=0) → WOKEN; catUpdate(true,9999)=WOKEN; catUpdate(true,10000)=SLEEPING
test_cat_pet_day: catOnPet(0); update(false,1999)=PETTED; update(false,2000)=IDLE
test_cat_pet_night: SLEEPING; catOnPet(0); update(true,2000)=WOKEN; update(true,10000)=SLEEPING
test_cat_no_blink_when_sleeping: SLEEPING → catBlinking(any)=false
test_cat_blink_period: IDLE since 0 → catBlinking(4000)=true, (4100)=true, (4200)=false
```
- [ ] **Step 2:** `pio test -e native` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `pio test -e native` → PASS.
- [ ] **Step 5: Commit** `feat: cat mood state machine`.

### Task 5: Hardware drivers — DHT11, touch, backlight

**Files:** Create `firmware/src/hw/sensors.h/.cpp`, `firmware/src/hw/touch.h/.cpp`, `firmware/src/hw/backlight.h/.cpp`; Create `firmware/WIRING.md`.

**Interfaces — Consumes:** `RoomReading`, `roomAccept` (Task 1). **Produces:**
```cpp
void sensorsInit();
void sensorsPoll(RoomReading& r, uint32_t nowMs); // reads DHT every DHT_READ_MS, calls roomAccept
void touchInit();
bool touchPressed();                               // digitalRead(PIN_TOUCH) == HIGH
void backlightInit();                              // no-op if BACKLIGHT_PIN < 0
void backlightSet(uint8_t level);                  // 0–255, LEDC PWM; no-op if BACKLIGHT_PIN < 0
```
- [ ] **Step 1:** Implement with `DHT dht(PIN_DHT, DHT11)`; `pinMode(PIN_TOUCH, INPUT)`; LEDC channel for backlight.
- [ ] **Step 2:** `WIRING.md`: DHT11 3-pin breakout (VCC→3V3, GND, DATA→GPIO27) and bare 4-pin (pin1 VCC, pin2 DATA + 10 kΩ to 3V3, pin3 NC, pin4 GND); TTP223 (VCC→3V3, GND, I/O→GPIO32); optional backlight (TFT BL → a free GPIO, e.g. 25, set `BACKLIGHT_PIN 25`).
- [ ] **Step 3:** `pio run -e esp32dev` → SUCCESS.
- [ ] **Step 4 (owner, board):** temporary `Serial.printf` in `loop()` of temp/hum/touch; flash; breathe on sensor (humidity rises), touch module (prints 1). Remove debug print after.
- [ ] **Step 5: Commit** `feat: dht11, touch and backlight drivers`.

### Task 6: Network worker on core 0 + clock

**Files:** Create `firmware/src/net/networker.h/.cpp`; Modify `firmware/src/network.h/.cpp` (replace NTPClient with `configTime`), `firmware/platformio.ini` (drop `NTPClient`), `firmware/src/main.ino`.

**Interfaces — Consumes:** existing `connectWiFi`, `fetchTasks`, `fetchNowPlaying`, `fetchArtwork`, `saveTasks`, `loadTasks`, `sortTasks`. **Produces:**
```cpp
// network.h (replaces syncNTP/getCurrentTime)
void   clockBegin();                       // configTime(TZ_OFFSET*3600, 0, NTP_SERVER)
bool   clockGetHM(int* hour, int* minute); // getLocalTime(&tm, 0); false if never synced
// networker.h
struct NetSnapshot {
  bool online; int taskCount; TaskItem tasks[MAX_TASKS];
  bool playing; NowPlaying np; uint32_t songVersion;   // ++ on song change
};
void netStart(const TaskItem* cached, int cachedCount); // xTaskCreatePinnedToCore(..., core 0)
void netGetSnapshot(NetSnapshot* out);                  // memcpy under mutex
int  netCopyArtwork(uint8_t* dst, int maxLen, uint32_t* version); // bytes copied, 0 if none
```
Worker loop: tasks every `POLL_INTERVAL_MS` (save to SPIFFS on success), Spotify every `NOWPLAYING_POLL_MS`, artwork download on song change into worker-owned buffer, `vTaskDelay(50ms)` between iterations, WiFi reconnect on drop. HTTP timeout ≤ 3000 ms. Time handled by ESP SNTP (thread-safe).

- [ ] **Step 1:** Implement clock + worker; `main.ino` temporarily keeps old screens but reads data only from `netGetSnapshot` (no HTTP in `loop()`).
- [ ] **Step 2:** `pio run -e esp32dev` → SUCCESS; `pio test -e native` still PASS.
- [ ] **Step 3 (owner, board):** add task via Telegram → appears ≤ 5 s; Spotify still shows; pull WiFi (router off) → header OFFLINE, cached tasks remain.
- [ ] **Step 4: Commit** `refactor: move networking to core-0 worker`.

### Task 7: Theme, header, page dots, Tareas screen

**Files:** Create `firmware/src/ui/theme.h`, `firmware/src/ui/chrome.h/.cpp`, `firmware/src/ui/screen_tasks.h/.cpp`; Modify `firmware/src/display.h/.cpp` (keep `displayInit`, `drawSplash`, `renderArtwork`; remove old `redrawAll`).

**Interfaces — Produces:**
```cpp
// theme.h: RGB565 constants C_BG(#0b0d12) C_CARD(#161a24) C_MUTED(#6b7385) C_TEXT(#e8e8e8)
//          C_COLD(#5ab0ff) C_OK(#4fd18b) C_HOT(#ff8a3d) C_URGENT(#ff5c5c); CARD_R 8, MARGIN 8, HEADER_H 26
uint16_t comfortColor(Comfort c);        // UNKNOWN → C_OK
const char* comfortWord(Comfort c);      // "fresco"/"bien"/"calor"/"--"
// chrome.h
struct HeaderInfo { const char* title; bool online; bool timeKnown; int hour, minute; bool playing; const RoomReading* room; bool roomFresh; };
void drawHeader(const HeaderInfo& h);            // title left, HH:MM or red OFFLINE centre, ♫, "22° · 48%" or "--° · --%" right
void drawPageDots(int index, int count);
// screen_tasks.h
void drawTasksScreen(const TaskItem* tasks, int count); // body only, cards per spec, "+N más"
```
Title for Tareas is `TAREAS · N` (pending count) — caller builds it. ♫: TFT fonts lack the glyph; draw a small note with primitives.
- [ ] **Step 1:** Implement; wire into `main.ino` so Tareas shows new style.
- [ ] **Step 2:** `pio run -e esp32dev` → SUCCESS.
- [ ] **Step 3 (owner, board):** compare against mockup `estilo-pantallas.html` option B: urgent red dot + "urgente" chip, done struck/dimmed, header temp/hum.
- [ ] **Step 4: Commit** `feat: soft-card theme, header and tasks screen`.

### Task 8: Spotify screen restyle

**Files:** Create `firmware/src/ui/screen_music.h/.cpp`; Modify `firmware/src/display.cpp` (rounded artwork corners mask optional).

**Interfaces — Consumes:** `NowPlaying`, `netCopyArtwork`, `renderArtwork`, `drawHeader`. **Produces:**
```cpp
void drawMusicScreen(const NowPlaying& np, const uint8_t* art, int artLen); // full body
void drawMusicProgress(const NowPlaying& np);                               // only bar + times region
```
Main redraws full body on song change / screen entry, otherwise only `drawMusicProgress` each Spotify update (no flicker).
- [ ] **Step 1:** Implement per style (artwork left, bold title, green artist, muted album, rounded 5 px bar).
- [ ] **Step 2:** `pio run -e esp32dev` → SUCCESS.
- [ ] **Step 3 (owner, board):** progress moves without screen flashing; song change updates art.
- [ ] **Step 4: Commit** `feat: restyled now playing screen`.

### Task 9: Habitación screen + cat drawing

**Files:** Create `firmware/src/ui/cat_draw.h/.cpp`, `firmware/src/ui/screen_room.h/.cpp`.

**Interfaces — Consumes:** `CatState`, `catBlinking`, `HumidMood`, `Comfort`, `comfortColor`, `comfortWord`, `RoomReading`. **Produces:**
```cpp
struct CatPose { CatMood mood; bool blink; HumidMood humid; uint16_t body; uint32_t nowMs; };
void catDrawInit();                       // creates TFT_eSprite 100x90 (16-bit); fallback: direct draw if createSprite fails
void catDraw(const CatPose& p, int x, int y); // draws into sprite, pushSprite at (x,y)
void drawRoomStrip(const RoomReading& r, bool fresh);   // bottom 56 px: TEMP, HUMEDAD, CONFORT; redraw only when shown values change
void drawRoomScreen();                    // clears body once on entry
```
Drawing: ears = triangles, body = rounded shape, eyes circles (lines when blink, `^ ^` when PETTED), nose/mouth, whiskers. SLEEPING: curled ellipse in body colour darkened ~50 %, closed-eye arcs, "Zzz" drifting up (y offset from `nowMs`). PETTED: hop offset (sin over `PET_MS`, max ~6 px). HUMID: sweat drop; DRY: open mouth/tongue. Cat area must sit fully between header and strip (y 26…184). Redraw cat at ~10–15 fps only on Habitación.
- [ ] **Step 1:** Implement.
- [ ] **Step 2:** `pio run -e esp32dev` → SUCCESS; check free heap printed at boot ≥ 40 KB after sprite + artwork buffers.
- [ ] **Step 3: Commit** `feat: room screen and animated cat`.

### Task 10: Integrate main loop

**Files:** Modify `firmware/src/main.ino`; delete obsolete `ScreenMode` from `nowplaying.h`.

**Interfaces — Consumes:** everything above.

`loop()` order each iteration: `sensorsPoll` → `GestureDetector::update(touchPressed(), now)` → on TAP `uiOnTap` (if `WOKE_CAT` → `catOnWakeTap`) → on LONG_PRESS while ROOM → `catOnPet` → `netGetSnapshot` → `clockGetHM` → `catUpdate(cat, isSleepHour(timeKnown ? hour : -1), now)` → `uiTick` → backlight (`backlightSet(catIsAsleep ? 40 : 255)`) → render: full redraw on screen change; header when minute/online/playing/shown temp/hum change; current screen's partial update. Boot: splash, SPIFFS cached tasks, WiFi, `clockBegin`, `netStart`, start on ROOM.
- [ ] **Step 1:** Implement.
- [ ] **Step 2:** `pio run -e esp32dev` → SUCCESS; `pio test -e native` → PASS.
- [ ] **Step 3 (owner, board) acceptance:** boot lands on cat; tap cycles Habitación→Tareas→(Spotify if playing)→Habitación; long press makes cat hop; leave on Tareas 30 s → back to cat; warm sensor in hand → cat turns orange (>25°); change `SLEEP_START_H` to current hour, reflash → cat sleeps, first tap wakes ~10 s; song start shows ♫ but does not switch screen; taps responsive while WiFi is down.
- [ ] **Step 4: Commit** `feat: room cat panel with touch navigation`.
