# ESP32 Task Panel

A personal task display panel built with an ESP32 and a 320x240 TFT screen. Tasks are managed via a Telegram bot and rendered on the screen in near real time. The backend runs on a self-hosted server and persists tasks in SQLite.

## Overview

```
Telegram app
    |
    | webhook (HTTPS)
    v
FastAPI backend (OCI / any VPS)
    |
    | HTTPS polling every 5s
    v
ESP32 + ST7789 TFT display
```

Tasks are added, completed, or deleted by sending commands to the Telegram bot. The ESP32 polls the API every 5 seconds and updates the display. Tasks persist both in the server database and in a local SPIFFS cache on the ESP32, so the last known state survives a reboot or network outage.

## Features

- Add tasks by plain text message (append `!` for urgent priority)
- `/add`, `/done`, `/delete`, `/list`, `/clear` bot commands
- Priority sorting: urgent pending tasks appear first
- Color-coded cards on the display (red = urgent, green = pending, grey = done)
- Offline resilience: SPIFFS cache on device, NTP clock shown in header
- API key authentication for the REST endpoint
- Webhook secret token validation for Telegram

## Repository Structure

```
backend/          FastAPI application
  main.py         App entry point, routes, lifespan
  bot.py          Telegram bot handlers
  database.py     aiosqlite CRUD
  auth.py         API key and webhook token verification
  models.py       Task dataclass
  tests/          pytest test suite
  requirements.txt
  esp32-tasks.service  systemd unit file

firmware/
  src/
    main.ino      Arduino entry point
    display.cpp/h TFT rendering
    network.cpp/h WiFi, NTP, HTTP fetch
    tasks.cpp/h   Task struct, SPIFFS persistence, sort
    config.h      Timing constants, display geometry
    secrets.h     Credentials (gitignored)
    User_Setup.h  TFT_eSPI pin and driver configuration
  platformio.ini
```

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32 (tested on ESP32 Dev Module) |
| Display | ST7789 320x240 TFT (landscape, setRotation 1) |
| SPI pins | MOSI=23, SCLK=18, CS=15, DC=2, RST=4, MISO=19 |
| SPI frequency | 27 MHz |

## Backend Setup

Requirements: Python 3.9+, a domain with HTTPS, a Telegram bot token from @BotFather.

### 1. Clone and install

```bash
git clone https://github.com/anxelito/EspNotificaciones esp-tasks
cd esp-tasks/backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 2. Environment variables

Create `backend/.env`:

```env
TELEGRAM_BOT_TOKEN=<token from BotFather>
TELEGRAM_SECRET_TOKEN=<random string, used to verify webhook calls>
API_KEY=<random hex string, used by the ESP32>
BASE_URL=https://your-domain.com
DATABASE_URL=/absolute/path/to/tasks.db
```

Generate random secrets:

```bash
python3 -c "import secrets; print(secrets.token_hex(32))"
```

### 3. Run with systemd

Edit `esp32-tasks.service` to match your paths and username, then:

```bash
sudo cp esp32-tasks.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now esp32-tasks
```

### 4. Reverse proxy (Apache)

Add to the HTTPS VirtualHost for your domain:

```apache
ProxyPreserveHost On
ProxyPass /health  http://127.0.0.1:8000/health
ProxyPassReverse /health  http://127.0.0.1:8000/health
ProxyPass /tasks   http://127.0.0.1:8000/tasks
ProxyPassReverse /tasks   http://127.0.0.1:8000/tasks
ProxyPass /webhook http://127.0.0.1:8000/webhook
ProxyPassReverse /webhook http://127.0.0.1:8000/webhook
```

Enable modules if needed: `sudo a2enmod proxy proxy_http`

A dedicated subdomain (e.g. `tareas.your-domain.com`) is cleaner if the root domain serves another application.

### 5. Register Telegram webhook

```bash
curl -X POST "https://api.telegram.org/bot<TOKEN>/setWebhook" \
  -H "Content-Type: application/json" \
  -d '{"url":"https://your-domain.com/webhook","secret_token":"<TELEGRAM_SECRET_TOKEN>"}'
```

### 6. Verify

```bash
curl https://your-domain.com/health
# {"status":"ok"}
```

## Firmware Setup

Requirements: PlatformIO (VSCode extension or CLI).

### 1. Create secrets.h

Copy the template and fill in your values:

```cpp
// firmware/src/secrets.h
#define WIFI_SSID   "your_ssid"
#define WIFI_PASS   "your_password"
#define API_URL     "https://your-domain.com/tasks"
#define API_KEY     "same_value_as_backend_API_KEY"
#define TZ_OFFSET   1    // UTC+1 (CET), use 2 for CEST
```

This file is listed in `.gitignore` and will not be committed.

### 2. User_Setup.h

`firmware/src/User_Setup.h` must match your display wiring. The file in this repo is configured for a ST7789 240x320 panel on the default ESP32 VSPI pins. If your wiring differs, edit the pin definitions before building.

### 3. Build and flash

```bash
cd firmware
pio run --target upload
```

Or use the PlatformIO toolbar in VSCode: Build, then Upload.

### 4. Monitor serial output

```bash
pio device monitor
```

## API Reference

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/health` | none | Health check |
| GET | `/tasks` | `X-API-Key` header | Returns task list as JSON |
| POST | `/webhook` | `X-Telegram-Bot-Api-Secret-Token` header | Telegram webhook receiver |

### Task object

```json
{
  "id": 1,
  "text": "Buy parts",
  "priority": "urgent",
  "status": "pending",
  "time_display": "14:30 16/05"
}
```

## Bot Commands

| Command | Description |
|---------|-------------|
| `<text>` | Add normal priority task |
| `<text>!` | Add urgent priority task |
| `/add <text> [urgent]` | Add task with optional priority flag |
| `/done <id>` | Mark task as done |
| `/delete <id>` | Delete task |
| `/list` | List all tasks |
| `/clear` | Delete all completed tasks |
| `/start` | Show help |

## Running Tests

```bash
cd backend
source venv/bin/activate
pytest tests/ -v
```

## Security Notes

- Port 8000 must not be exposed to the internet. Bind uvicorn to `127.0.0.1` only.
- The ESP32 uses `WiFiClientSecure` with `setInsecure()`, which skips certificate verification. This is acceptable for a private endpoint behind Cloudflare but should be noted.
- Secrets are kept out of version control via `.gitignore`. The `.env` file and `secrets.h` must be created manually on each deployment.

## License

MIT
