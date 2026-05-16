# Dashboard Panels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new display panels (iCloud calendar events and Open-Meteo weather + clock) that rotate with the existing tasks panel every 5 seconds on the ESP32.

**Architecture:** The backend gains a `/calendar` endpoint that fetches today and tomorrow's events from iCloud via CalDAV (using the `caldav` Python library in a thread to avoid blocking the async loop) and returns clean JSON authenticated with the existing API key. The ESP32 polls `/calendar` every 5 minutes and Open-Meteo directly every 10 minutes, caching results in RAM. A timer in `loop()` switches `currentPanel` (0=tasks, 1=calendar, 2=weather) every 5 seconds and redraws.

**Tech Stack:** Python caldav 1.3.9, vobject, asyncio.to_thread, Open-Meteo REST API (no auth), TFT_eSPI GFX fonts, ArduinoJson 7.

---

## File Map

**New files:**
- `backend/calendar_client.py` — CalDAV fetch + parse, returns `list[dict]`
- `backend/tests/test_calendar.py` — tests for `/calendar` endpoint
- `firmware/src/weather.h` — `WeatherData` struct

**Modified files:**
- `backend/requirements.txt` — add `caldav==1.3.9`
- `backend/main.py` — add `GET /calendar` route
- `backend/.env` (server) — add `CALDAV_USERNAME`, `CALDAV_PASSWORD`
- `firmware/src/config.h` — add `PANEL_INTERVAL_MS`, `CALENDAR_POLL_MS`, `WEATHER_POLL_MS`, `MAX_EVENTS`
- `firmware/src/secrets.h` — add `WEATHER_LAT`, `WEATHER_LON`
- `firmware/src/tasks.h` — add `CalEvent` struct
- `firmware/src/network.h/.cpp` — add `fetchCalendar()`, `fetchWeather()`
- `firmware/src/display.h/.cpp` — add `drawCalendarPanel()`, `drawWeatherPanel()`, `drawPanel()`
- `firmware/src/main.ino` — add panel rotation state and timers

---

## Task 1: Backend — CalDAV client

**Files:**
- Modify: `backend/requirements.txt`
- Create: `backend/calendar_client.py`

- [ ] **Step 1: Add caldav to requirements**

```text
# backend/requirements.txt  (append)
caldav==1.3.9
```

- [ ] **Step 2: Install on dev machine**

```bash
cd backend
source venv/bin/activate
pip install caldav==1.3.9
```

Expected: installs caldav and its deps (vobject, requests, lxml).

- [ ] **Step 3: Create calendar_client.py**

```python
# backend/calendar_client.py
import os
import asyncio
from datetime import datetime, timedelta, date

import caldav


def _parse_event(event, today: date) -> dict | None:
    try:
        vevent = event.vobject_instance.vevent
        summary = str(vevent.summary.value)
        dtstart = vevent.dtstart.value

        if isinstance(dtstart, datetime):
            if dtstart.tzinfo is not None:
                dtstart = dtstart.astimezone().replace(tzinfo=None)
            time_display = dtstart.strftime("%H:%M")
            day = 0 if dtstart.date() == today else 1
        else:
            # all-day: dtstart is a date object
            time_display = "Todo el dia"
            day = 0 if dtstart == today else 1

        return {
            "summary": summary[:40],
            "time_display": time_display,
            "day": day,
        }
    except Exception:
        return None


def fetch_events() -> list[dict]:
    username = os.getenv("CALDAV_USERNAME", "")
    password = os.getenv("CALDAV_PASSWORD", "")
    if not username or not password:
        return []

    try:
        client = caldav.DAVClient(
            url="https://caldav.icloud.com",
            username=username,
            password=password,
        )
        principal = client.principal()
        calendars = principal.calendars()

        today = datetime.now().date()
        start = datetime.combine(today, datetime.min.time())
        end = start + timedelta(days=2)

        results = []
        for cal in calendars:
            try:
                for event in cal.date_search(start=start, end=end, expand=True):
                    parsed = _parse_event(event, today)
                    if parsed is not None:
                        results.append(parsed)
            except Exception:
                continue

        results.sort(key=lambda e: (e["day"], e["time_display"]))
        return results[:6]
    except Exception:
        return []


async def fetch_events_async() -> list[dict]:
    return await asyncio.to_thread(fetch_events)
```

- [ ] **Step 4: Add env vars to .env on server**

```env
CALDAV_USERNAME=tu_apple_id@icloud.com
CALDAV_PASSWORD=xxxx-xxxx-xxxx-xxxx
```

Generate the app-specific password at: appleid.apple.com → Sign-In & Security → App-Specific Passwords → Generate. Label it "ESP32 Panel".

- [ ] **Step 5: Commit**

```bash
git add backend/requirements.txt backend/calendar_client.py
git commit -m "feat: add iCloud CalDAV client"
```

---

## Task 2: Backend — /calendar endpoint and tests

**Files:**
- Modify: `backend/main.py`
- Create: `backend/tests/test_calendar.py`

- [ ] **Step 1: Write failing tests**

```python
# backend/tests/test_calendar.py
import pytest
import os
from unittest.mock import AsyncMock, patch, MagicMock

os.environ.setdefault("API_KEY", "test-key")
os.environ.setdefault("TELEGRAM_BOT_TOKEN", "123:test")
os.environ.setdefault("TELEGRAM_SECRET_TOKEN", "tg-secret")
os.environ.setdefault("DATABASE_URL", ":memory:")
os.environ.setdefault("BASE_URL", "https://example.com")

from httpx import AsyncClient, ASGITransport

MOCK_EVENTS = [
    {"summary": "Dentista", "time_display": "10:00", "day": 0},
    {"summary": "Reunion", "time_display": "15:00", "day": 1},
]


@pytest.fixture
async def client():
    with patch("main.build_application") as mock_build:
        mock_app = MagicMock()
        mock_app.initialize = AsyncMock()
        mock_app.bot.set_webhook = AsyncMock()
        mock_app.shutdown = AsyncMock()
        mock_app.process_update = AsyncMock()
        mock_build.return_value = mock_app

        from main import app
        app.state.bot_app = mock_app
        async with AsyncClient(
            transport=ASGITransport(app=app), base_url="http://test"
        ) as c:
            yield c


@pytest.mark.asyncio
async def test_calendar_missing_key(client):
    resp = await client.get("/calendar")
    assert resp.status_code == 422


@pytest.mark.asyncio
async def test_calendar_wrong_key(client):
    resp = await client.get("/calendar", headers={"X-API-Key": "wrong"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_calendar_returns_events(client):
    with patch("main.fetch_events_async", new_callable=AsyncMock, return_value=MOCK_EVENTS):
        resp = await client.get("/calendar", headers={"X-API-Key": "test-key"})
    assert resp.status_code == 200
    data = resp.json()
    assert len(data) == 2
    assert data[0]["summary"] == "Dentista"
    assert data[0]["day"] == 0


@pytest.mark.asyncio
async def test_calendar_returns_empty_on_error(client):
    with patch("main.fetch_events_async", new_callable=AsyncMock, return_value=[]):
        resp = await client.get("/calendar", headers={"X-API-Key": "test-key"})
    assert resp.status_code == 200
    assert resp.json() == []
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
cd backend
pytest tests/test_calendar.py -v
```

Expected: ImportError or 404 (route doesn't exist yet).

- [ ] **Step 3: Add /calendar route to main.py**

Add import at top of `backend/main.py` (after existing imports):

```python
from calendar_client import fetch_events_async
```

Add route after the existing `/tasks` route:

```python
@app.get("/calendar")
async def calendar_endpoint(_=Depends(verify_api_key)):
    return await fetch_events_async()
```

- [ ] **Step 4: Run tests — expect pass**

```bash
pytest tests/test_calendar.py -v
```

Expected: 4 passed.

- [ ] **Step 5: Run full test suite to check no regressions**

```bash
pytest tests/ -v
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add backend/main.py backend/tests/test_calendar.py
git commit -m "feat: add /calendar endpoint with iCloud CalDAV"
```

---

## Task 3: Firmware — New structs and config constants

**Files:**
- Modify: `firmware/src/config.h`
- Modify: `firmware/src/secrets.h`
- Modify: `firmware/src/tasks.h`
- Create: `firmware/src/weather.h`

- [ ] **Step 1: Add timing constants and MAX_EVENTS to config.h**

Add to `firmware/src/config.h` after the existing defines:

```cpp
#define PANEL_INTERVAL_MS    5000UL
#define CALENDAR_POLL_MS   300000UL
#define WEATHER_POLL_MS    600000UL
#define MAX_EVENTS           6
```

- [ ] **Step 2: Add weather coordinates to secrets.h**

Add to `firmware/src/secrets.h`:

```cpp
#define WEATHER_LAT  "42.34"   // replace with your latitude
#define WEATHER_LON  "-8.71"   // replace with your longitude
```

Find your coordinates at maps.google.com (right-click → What's here).

- [ ] **Step 3: Add CalEvent struct to tasks.h**

Add after the existing `TaskItem` struct in `firmware/src/tasks.h`:

```cpp
struct CalEvent {
    char    summary[48];
    char    time_display[12];
    uint8_t day;  // 0 = today, 1 = tomorrow
};
```

- [ ] **Step 4: Create weather.h**

```cpp
// firmware/src/weather.h
#pragma once

struct WeatherData {
    float temp;
    int   weathercode;
    char  description[20];
};
```

- [ ] **Step 5: Build to verify no compile errors**

In VSCode PlatformIO: Build (Ctrl+Alt+B). Expected: SUCCESS, no errors.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/config.h firmware/src/secrets.h firmware/src/tasks.h firmware/src/weather.h
git commit -m "feat: add CalEvent, WeatherData structs and panel timing constants"
```

---

## Task 4: Firmware — fetchCalendar()

**Files:**
- Modify: `firmware/src/network.h`
- Modify: `firmware/src/network.cpp`

- [ ] **Step 1: Add fetchCalendar declaration to network.h**

Add to `firmware/src/network.h` (after the existing `fetchTasks` declaration):

```cpp
#include "tasks.h"

int fetchCalendar(CalEvent* events);
```

If `tasks.h` is already included earlier in network.h, do not add a second include. Check the existing includes first.

- [ ] **Step 2: Implement fetchCalendar in network.cpp**

Add to `firmware/src/network.cpp` after `fetchTasks`:

```cpp
int fetchCalendar(CalEvent* events) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(API_URL);
    // Replace /tasks with /calendar
    url.replace("/tasks", "/calendar");
    http.begin(client, url);
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return -1;

    JsonArray arr = doc.as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        if (count >= MAX_EVENTS) break;
        strlcpy(events[count].summary,      obj["summary"]      | "", sizeof(events[count].summary));
        strlcpy(events[count].time_display, obj["time_display"] | "", sizeof(events[count].time_display));
        events[count].day = obj["day"] | 0;
        count++;
    }
    return count;
}
```

- [ ] **Step 3: Build to verify**

PlatformIO Build. Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/network.h firmware/src/network.cpp
git commit -m "feat: add fetchCalendar() to network layer"
```

---

## Task 5: Firmware — fetchWeather()

**Files:**
- Modify: `firmware/src/network.h`
- Modify: `firmware/src/network.cpp`

- [ ] **Step 1: Add fetchWeather declaration to network.h**

Add include and declaration to `firmware/src/network.h`:

```cpp
#include "weather.h"

bool fetchWeather(WeatherData* weather);
```

- [ ] **Step 2: Add weather code → description helper and fetchWeather in network.cpp**

Add to `firmware/src/network.cpp`:

```cpp
static const char* _weatherDesc(int code) {
    if (code == 0)        return "Despejado";
    if (code <= 3)        return "Nubes";
    if (code <= 48)       return "Niebla";
    if (code <= 55)       return "Llovizna";
    if (code <= 65)       return "Lluvia";
    if (code <= 75)       return "Nieve";
    if (code <= 82)       return "Chubascos";
    if (code <= 99)       return "Tormenta";
    return "Desconocido";
}

bool fetchWeather(WeatherData* weather) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast"
                 "?latitude=" WEATHER_LAT
                 "&longitude=" WEATHER_LON
                 "&current=temperature_2m,weathercode"
                 "&timezone=auto";
    http.begin(client, url);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    JsonObject current = doc["current"];
    weather->temp        = current["temperature_2m"] | 0.0f;
    weather->weathercode = current["weathercode"]    | 0;
    strlcpy(weather->description, _weatherDesc(weather->weathercode), sizeof(weather->description));
    return true;
}
```

- [ ] **Step 3: Build to verify**

PlatformIO Build. Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/network.h firmware/src/network.cpp
git commit -m "feat: add fetchWeather() via Open-Meteo"
```

---

## Task 6: Firmware — Calendar display panel

**Files:**
- Modify: `firmware/src/display.h`
- Modify: `firmware/src/display.cpp`

- [ ] **Step 1: Add declaration to display.h**

Add to `firmware/src/display.h`:

```cpp
#include "tasks.h"
#include "weather.h"

void drawCalendarPanel(CalEvent* events, int count, const String& time_str, bool online);
```

(Add includes only if not already present.)

- [ ] **Step 2: Implement drawCalendarPanel in display.cpp**

Add to `firmware/src/display.cpp`:

```cpp
void drawCalendarPanel(CalEvent* events, int count, const String& time_str, bool online) {
    tft.fillScreen(C_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("CALENDARIO", 8, 8, 2);
    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawRightString(online ? time_str : "OFFLINE", SCREEN_W - 6, 8, 2);

    if (count == 0) {
        tft.setTextColor(TFT_DARKGREY, C_BG);
        tft.drawCentreString("Sin eventos", SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
        return;
    }

    int y = HEADER_H + 6;
    bool shownToday    = false;
    bool shownTomorrow = false;

    for (int i = 0; i < count && y < SCREEN_H - 16; i++) {
        CalEvent& e = events[i];

        if (e.day == 0 && !shownToday) {
            tft.setTextColor(TFT_CYAN, C_BG);
            tft.drawString("HOY", 8, y, 2);
            y += 18;
            shownToday = true;
        }
        if (e.day == 1 && !shownTomorrow) {
            tft.setTextColor(TFT_CYAN, C_BG);
            tft.drawString("MANANA", 8, y, 2);
            y += 18;
            shownTomorrow = true;
        }

        // Time badge
        tft.setTextColor(TFT_DARKGREY, C_BG);
        tft.drawString(e.time_display, 8, y + 2, 1);

        // Summary — truncate to ~24 chars for font 2
        String txt = String(e.summary);
        if (txt.length() > 22) txt = txt.substring(0, 21) + "~";
        tft.setTextColor(TFT_WHITE, C_BG);
        tft.drawString(txt, 70, y, 2);

        y += 22;
    }
}
```

- [ ] **Step 3: Build to verify**

PlatformIO Build. Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat: add drawCalendarPanel()"
```

---

## Task 7: Firmware — Weather display panel

**Files:**
- Modify: `firmware/src/display.h`
- Modify: `firmware/src/display.cpp`

- [ ] **Step 1: Add declaration to display.h**

```cpp
void drawWeatherPanel(const WeatherData& weather, const String& time_str, bool online);
```

- [ ] **Step 2: Implement drawWeatherPanel in display.cpp**

Add to `firmware/src/display.cpp`:

```cpp
void drawWeatherPanel(const WeatherData& weather, const String& time_str, bool online) {
    tft.fillScreen(C_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("TIEMPO", 8, 8, 2);
    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawRightString(online ? time_str : "OFFLINE", SCREEN_W - 6, 8, 2);

    // Large time — font 6 is 48px, digits only (perfect for HH:MM)
    tft.setTextColor(TFT_WHITE, C_BG);
    tft.drawCentreString(time_str, SCREEN_W / 2, HEADER_H + 10, 6);

    // Temperature
    String tempStr = String(weather.temp, 1) + "C";
    tft.setTextColor(TFT_CYAN, C_BG);
    tft.drawCentreString(tempStr, SCREEN_W / 2, HEADER_H + 72, 4);

    // Weather description
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString(weather.description, SCREEN_W / 2, HEADER_H + 104, 2);
}
```

- [ ] **Step 3: Build to verify**

PlatformIO Build. Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat: add drawWeatherPanel()"
```

---

## Task 8: Firmware — Panel rotation in main.ino

**Files:**
- Modify: `firmware/src/display.h`
- Modify: `firmware/src/display.cpp`
- Modify: `firmware/src/main.ino`

- [ ] **Step 1: Add drawPanel() declaration to display.h**

```cpp
void drawPanel(int panel, TaskItem* tasks, int taskCount,
               CalEvent* events, int calCount,
               const WeatherData& weather,
               const String& time_str, bool online);
```

- [ ] **Step 2: Implement drawPanel() in display.cpp**

Add to `firmware/src/display.cpp`:

```cpp
void drawPanel(int panel, TaskItem* tasks, int taskCount,
               CalEvent* events, int calCount,
               const WeatherData& weather,
               const String& time_str, bool online) {
    switch (panel) {
        case 0: redrawAll(tasks, taskCount, time_str, online); break;
        case 1: drawCalendarPanel(events, calCount, time_str, online); break;
        case 2: drawWeatherPanel(weather, time_str, online);   break;
    }
}
```

- [ ] **Step 3: Rewrite main.ino with full panel rotation logic**

Replace the full contents of `firmware/src/main.ino`:

```cpp
// firmware/src/main.ino
#include "config.h"
#include "secrets.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include "weather.h"
#include <SPIFFS.h>

static TaskItem   tasks[MAX_TASKS];
static int        taskCount       = 0;
static CalEvent   calEvents[MAX_EVENTS];
static int        calEventCount   = 0;
static WeatherData weather        = {0.0f, 0, "---"};

static bool         online         = false;
static int          currentPanel   = 0;
static unsigned long lastPoll       = 0;
static unsigned long lastNTP        = 0;
static unsigned long lastCalPoll    = 0;
static unsigned long lastWeatherPoll = 0;
static unsigned long lastPanelSwitch = 0;

static String currentTime() {
    return online ? getCurrentTime() : "--:--";
}

void setup() {
    Serial.begin(115200);

    displayInit();
    drawSplash("Iniciando...");

    SPIFFS.begin(true);
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

        drawSplash("Cargando datos...");
        calEventCount = fetchCalendar(calEvents);
        if (calEventCount < 0) calEventCount = 0;
        fetchWeather(&weather);
        lastCalPoll    = millis();
        lastWeatherPoll = millis();
    }

    drawPanel(currentPanel, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
    lastPanelSwitch = millis();
}

void loop() {
    unsigned long now = millis();

    // NTP re-sync
    if (online && (now - lastNTP >= NTP_SYNC_INTERVAL)) {
        syncNTP();
        lastNTP = now;
    }

    // Panel rotation
    if (now - lastPanelSwitch >= PANEL_INTERVAL_MS) {
        currentPanel = (currentPanel + 1) % 3;
        lastPanelSwitch = now;
        drawPanel(currentPanel, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
    }

    // Task poll
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
        drawPanel(currentPanel, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
        lastPoll = now;
    }

    // Calendar poll
    if (online && (now - lastCalPoll >= CALENDAR_POLL_MS)) {
        int result = fetchCalendar(calEvents);
        if (result >= 0) calEventCount = result;
        lastCalPoll = now;
    }

    // Weather poll
    if (online && (now - lastWeatherPoll >= WEATHER_POLL_MS)) {
        fetchWeather(&weather);
        lastWeatherPoll = now;
    }
}
```

- [ ] **Step 4: Build**

PlatformIO Build. Expected: SUCCESS.

- [ ] **Step 5: Flash and verify**

PlatformIO Upload. On boot you should see:
- Splash screen sequence
- Panel 0 (tasks) for 5 seconds
- Panel 1 (calendar) for 5 seconds
- Panel 2 (weather + large clock) for 5 seconds
- Repeat

- [ ] **Step 6: Deploy backend changes to server**

```bash
# On server
cd esp-tasks
git pull
source backend/venv/bin/activate
pip install -r backend/requirements.txt
# Edit .env to add CALDAV_USERNAME and CALDAV_PASSWORD
nano backend/.env
sudo systemctl restart esp32-tasks
curl https://tareas.anxelito.com/calendar -H "X-API-Key: YOUR_KEY"
```

Expected: JSON array (may be empty if credentials not set yet).

- [ ] **Step 7: Commit**

```bash
git add firmware/src/display.h firmware/src/display.cpp firmware/src/main.ino
git commit -m "feat: rotating dashboard panels — tasks, calendar, weather"
```
