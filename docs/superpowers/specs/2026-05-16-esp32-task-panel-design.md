# ESP32 Smart Task Panel — Design Spec
**Date:** 2026-05-16  
**Status:** Approved

## Overview

Physical task dashboard: TFT screen on ESP32 controlled via Telegram bot. Tasks sent from phone/PC appear on screen in real time. Backend runs as a service on OCI (Oracle Cloud Infrastructure).

---

## Architecture

```
[Telegram]
    │  webhook POST /webhook (secret token validated)
    ▼
[FastAPI + uvicorn on OCI]
    │  SQLite (aiosqlite) — persists across weekly restarts
    │
    │  GET /tasks (HTTPS, X-API-Key header)
    ▼
[ESP32 + TFT 320×240]
    └── SPIFFS cache — survives ESP32 reboot/offline
```

### Data flow — add task
1. User sends text or `/add` command to Telegram bot
2. Telegram pushes to `POST /webhook`
3. FastAPI saves to SQLite
4. ESP32 next poll (≤5s) fetches updated list → redraws screen

### Data flow — complete task
1. User sends `/done <id>` to Telegram
2. FastAPI updates SQLite `status = done`
3. ESP32 next poll → redraws with grey bar

---

## Backend — FastAPI

### Endpoints

| Method | Path | Auth | Purpose |
|---|---|---|---|
| POST | `/webhook` | Telegram secret token | Receive bot commands |
| GET | `/tasks` | `X-API-Key` header | ESP32 fetch task list |
| GET | `/health` | none | OCI health check |

### Task schema (SQLite)

```sql
CREATE TABLE tasks (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    text         TEXT NOT NULL,
    priority     TEXT DEFAULT 'normal',   -- 'normal' | 'urgent'
    status       TEXT DEFAULT 'pending',  -- 'pending' | 'done'
    created_at   DATETIME DEFAULT CURRENT_TIMESTAMP,
    time_display TEXT                     -- "14:30 16/05" pre-formatted for ESP32
);
```

### JSON response (`/tasks`)

```json
[
  {"id":1,"text":"Comprar resistencias","priority":"urgent","status":"pending","time_display":"14:30 16/05"},
  {"id":2,"text":"Soldar pines","priority":"normal","status":"pending","time_display":"14:45 16/05"},
  {"id":3,"text":"Instalar IDE","priority":"normal","status":"done","time_display":"11:00 16/05"}
]
```

### Telegram bot commands

| Command / Input | Action |
|---|---|
| `/add <text>` | Add normal-priority task |
| `/add <text> urgent` | Add urgent task |
| `<plain text>` | Add normal-priority task |
| `<plain text>!` (ends with `!`) | Add urgent task — the `!` is stripped from stored text |
| `/done <id>` | Mark task done |
| `/delete <id>` | Delete task |
| `/list` | Show all tasks with IDs |
| `/clear` | Remove all done tasks |

### Secrets (`.env`)

```
TELEGRAM_BOT_TOKEN=...
TELEGRAM_SECRET_TOKEN=...
API_KEY=...
DATABASE_URL=./tasks.db
```

### Dependencies

```
fastapi
uvicorn
python-telegram-bot>=20
aiosqlite
python-dotenv
```

---

## ESP32 Firmware

### Libraries

| Library | Purpose |
|---|---|
| TFT_eSPI | TFT display driver (already working) |
| ArduinoJson | Parse JSON API response |
| WiFiClientSecure | HTTPS with TLS |
| NTPClient | Real-time clock |
| SPIFFS | Local task cache |

### Boot sequence

```
1. Init TFT → splash screen
2. Connect WiFi
3. Load /tasks.json from SPIFFS → draw immediately (offline-first)
4. Sync NTP (pool.ntp.org)
5. Loop every 5s:
   a. GET https://oci-host/tasks  (X-API-Key header)
   b. Success → update SPIFFS + redraw
   c. Failure → keep current screen + show OFFLINE dot in header
   d. Every 1h → re-sync NTP
```

### File structure

```
/
├── src/
│   ├── main.ino
│   ├── config.h       ← non-sensitive constants (public)
│   ├── secrets.h      ← sensitive values (gitignored)
│   ├── display.h/.cpp ← TFT drawing functions
│   ├── tasks.h/.cpp   ← task model + SPIFFS cache
│   └── network.h/.cpp ← WiFi, HTTPS, NTP
```

### `secrets.h` (gitignored)

```cpp
#define WIFI_SSID  "your_wifi"
#define WIFI_PASS  "your_pass"
#define API_URL    "https://your-oci-host/tasks"
#define API_KEY    "your_secret_key"
#define TZ_OFFSET  -5   // UTC offset
```

### `config.h` (public)

```cpp
#define POLL_INTERVAL_MS  5000
#define MAX_TASKS         8    // ESP32 holds at most 8 tasks in RAM
#define NTP_SYNC_INTERVAL 3600000
#define NTP_SERVER        "pool.ntp.org"
```

### Task ordering (display priority)

API returns all tasks. ESP32 renders first `MAX_TASKS` entries using this sort order:
1. Urgent + pending (top)
2. Normal + pending
3. Done (bottom, dimmed)

If total tasks exceed `MAX_TASKS`, lowest-priority visible items are dropped and "+N más" shown at bottom. Done tasks are dropped first.

---

## Display — TFT 320×240 (Minimal Style)

### Layout

```
┌─────────────────────────────────────┐  y=0
│  TAREAS              14:32  3 pend  │  y=0–28  header (dark grey bg)
├─────────────────────────────────────┤  y=28
│▌ Comprar resistencias               │  task card (barra roja = urgent)
│  14:30 16/05              URGENTE   │
├─────────────────────────────────────┤
│▌ Soldar pines header                │  task card (barra verde = normal pending)
│  14:45 16/05              PEND      │
├─────────────────────────────────────┤
│▌ Flashear firmware                  │
│  15:00 16/05              PEND      │
├─────────────────────────────────────┤
│  Instalar IDE               DONE    │  done task (dimmed, strikethrough)
│  11:00 16/05                        │
└─────────────────────────────────────┘
                              +2 más    y=235 (if overflow)
```

### Color coding

| State | Left bar | Text | Badge |
|---|---|---|---|
| urgent + pending | RED (`TFT_RED`) | White | "URGENTE" red |
| normal + pending | GREEN (`TFT_GREEN`) | White | "PEND" green |
| done | DARKGREY | Dimmed, strikethrough | "DONE" grey |

### Header

- Left: "TAREAS" white bold
- Center-right: NTP time green `HH:MM`
- Right: pending count grey
- OFFLINE mode: red dot replaces time when API unreachable

---

## Security

| Surface | Mechanism |
|---|---|
| Telegram → backend | Secret token header validated on every webhook |
| ESP32 → backend | `X-API-Key` header on every request |
| Transport | HTTPS/TLS (OCI load balancer terminates SSL) |
| Secrets | `.env` for backend, `secrets.h` gitignored for ESP32 |
| `.gitignore` | `secrets.h`, `.env`, `*.db` |

---

## Extras (v1 scope)

- **Priority:** urgent (red) / normal (green) via `!` suffix or `/add ... urgent`
- **NTP time:** shown in header, synced at boot + hourly
- **SPIFFS cache:** tasks survive ESP32 reboot; shown while API unreachable
- **Offline indicator:** red dot in header when polling fails

## Out of scope (v1)

- Buzzer
- Home Assistant integration
- Multi-user Telegram groups
- Task editing (delete + re-add as workaround)
