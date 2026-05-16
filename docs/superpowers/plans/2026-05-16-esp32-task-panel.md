# ESP32 Smart Task Panel — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a physical task dashboard (ESP32 + TFT 320×240) controlled via Telegram bot with a FastAPI backend on OCI.

**Architecture:** Telegram sends commands via webhook to FastAPI on OCI → tasks stored in SQLite → ESP32 polls HTTPS every 5s → draws tasks on TFT (minimal style, Layout A).

**Tech Stack:** Python 3.11+, FastAPI, python-telegram-bot v20, aiosqlite, pytest-asyncio / Arduino C++, TFT_eSPI, ArduinoJson, WiFiClientSecure, NTPClient, SPIFFS.

---

## File Map

```
EspNotificaciones/
├── backend/
│   ├── main.py          FastAPI app, lifespan, /tasks + /webhook + /health
│   ├── bot.py           Telegram handlers (all commands + plain-text)
│   ├── database.py      aiosqlite CRUD (init, add, get_all, set_done, delete, clear_done)
│   ├── models.py        Task dataclass + to_dict()
│   ├── auth.py          API-key dependency + Telegram secret-token check
│   ├── requirements.txt
│   ├── .env.example
│   ├── esp32-tasks.service   systemd unit for OCI
│   └── tests/
│       ├── conftest.py
│       ├── test_database.py
│       ├── test_api.py
│       └── test_bot.py
├── firmware/
│   ├── platformio.ini
│   └── src/
│       ├── main.ino     setup() + loop() — wires all modules
│       ├── config.h     non-sensitive constants
│       ├── secrets.h.example
│       ├── display.h / display.cpp   TFT drawing functions
│       ├── tasks.h / tasks.cpp       TaskItem struct, sort, SPIFFS cache
│       └── network.h / network.cpp   WiFi, HTTPS poll, NTP
├── .gitignore
└── docs/
```

---

## Task 1: Project Scaffold

**Files:**
- Create: `.gitignore`
- Create: `backend/requirements.txt`
- Create: `backend/.env.example`
- Create: `backend/tests/__init__.py` (empty)
- Create: `backend/tests/conftest.py`
- Create: `firmware/platformio.ini`
- Create: `firmware/src/secrets.h.example`

- [ ] **Step 1: Create .gitignore**

```
# Secrets
secrets.h
.env
*.db

# Python
__pycache__/
*.pyc
.venv/
venv/

# PlatformIO
firmware/.pio/
firmware/.vscode/

# Brainstorm artifacts
.superpowers/
```

- [ ] **Step 2: Create backend/requirements.txt**

```
fastapi==0.111.0
uvicorn[standard]==0.29.0
python-telegram-bot==21.3
aiosqlite==0.20.0
python-dotenv==1.0.1
pytest==8.2.0
pytest-asyncio==0.23.7
httpx==0.27.0
```

- [ ] **Step 3: Create backend/.env.example**

```
TELEGRAM_BOT_TOKEN=123456:ABCdef...
TELEGRAM_SECRET_TOKEN=random_32_char_string_here
API_KEY=another_random_32_char_string
DATABASE_URL=./tasks.db
BASE_URL=https://your-oci-domain.com
```

- [ ] **Step 4: Create backend/pytest.ini**

```ini
[pytest]
asyncio_mode = auto
```

- [ ] **Step 4b: Create backend/tests/conftest.py**

```python
# empty — pytest.ini handles asyncio_mode
```

- [ ] **Step 5: Create firmware/src/secrets.h.example**

```cpp
// Copy to secrets.h and fill in — secrets.h is gitignored
#define WIFI_SSID   "your_wifi_name"
#define WIFI_PASS   "your_wifi_password"
#define API_URL     "https://your-oci-domain.com/tasks"
#define API_KEY     "your_api_key_here"
#define TZ_OFFSET   -5   // UTC offset for your timezone
```

- [ ] **Step 6: Create firmware/platformio.ini**

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    bodmer/TFT_eSPI
    bblanchon/ArduinoJson@^7
    arduino-libraries/NTPClient
build_flags =
    -DUSER_SETUP_LOADED=1
```

> **Note:** TFT_eSPI pin config goes in `User_Setup.h` inside the library — copy your existing working setup from your PC Monitor project.

- [ ] **Step 7: Create directories and install Python deps**

```bash
cd backend
python -m venv venv
venv\Scripts\activate      # Windows
# source venv/bin/activate  # Linux/Mac
pip install -r requirements.txt
```

Expected: no errors, packages installed.

- [ ] **Step 8: Commit**

```bash
git init
git add .gitignore backend/requirements.txt backend/.env.example backend/pytest.ini backend/tests/__init__.py backend/tests/conftest.py firmware/platformio.ini firmware/src/secrets.h.example
git commit -m "chore: project scaffold"
```

---

## Task 2: Task Data Model

**Files:**
- Create: `backend/models.py`

- [ ] **Step 1: Write models.py**

```python
from dataclasses import dataclass
from datetime import datetime


@dataclass
class Task:
    id: int
    text: str
    priority: str        # 'normal' | 'urgent'
    status: str          # 'pending' | 'done'
    created_at: str      # stored as ISO string from SQLite
    time_display: str    # "14:30 16/05" pre-formatted

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "text": self.text,
            "priority": self.priority,
            "status": self.status,
            "time_display": self.time_display,
        }
```

- [ ] **Step 2: Verify import**

```bash
cd backend && python -c "from models import Task; print('OK')"
```

Expected: `OK`

- [ ] **Step 3: Commit**

```bash
git add backend/models.py
git commit -m "feat: task data model"
```

---

## Task 3: Database Layer

**Files:**
- Create: `backend/database.py`
- Create: `backend/tests/test_database.py`

- [ ] **Step 1: Write failing tests**

```python
# backend/tests/test_database.py
import pytest
import os
import database as db
from models import Task


@pytest.fixture(autouse=True)
async def temp_db(tmp_path):
    path = str(tmp_path / "test.db")
    await db.init_db(path)
    yield
    if os.path.exists(path):
        os.unlink(path)


@pytest.mark.asyncio
async def test_add_normal_task():
    task = await db.add_task("Buy components", "normal")
    assert task.text == "Buy components"
    assert task.priority == "normal"
    assert task.status == "pending"
    assert isinstance(task.id, int)


@pytest.mark.asyncio
async def test_add_urgent_task():
    task = await db.add_task("Fix now", "urgent")
    assert task.priority == "urgent"


@pytest.mark.asyncio
async def test_get_all_returns_tasks():
    await db.add_task("A", "normal")
    await db.add_task("B", "urgent")
    tasks = await db.get_all_tasks()
    assert len(tasks) == 2


@pytest.mark.asyncio
async def test_sort_urgent_first():
    await db.add_task("normal task", "normal")
    await db.add_task("urgent task", "urgent")
    tasks = await db.get_all_tasks()
    assert tasks[0].priority == "urgent"


@pytest.mark.asyncio
async def test_sort_done_last():
    t_normal = await db.add_task("normal", "normal")
    t_urgent = await db.add_task("urgent", "urgent")
    await db.set_task_done(t_urgent.id)
    tasks = await db.get_all_tasks()
    assert tasks[-1].status == "done"


@pytest.mark.asyncio
async def test_set_task_done():
    task = await db.add_task("Do it", "normal")
    done = await db.set_task_done(task.id)
    assert done is not None
    assert done.status == "done"


@pytest.mark.asyncio
async def test_set_done_not_found():
    result = await db.set_task_done(9999)
    assert result is None


@pytest.mark.asyncio
async def test_delete_task():
    task = await db.add_task("Delete me", "normal")
    result = await db.delete_task(task.id)
    assert result is True
    tasks = await db.get_all_tasks()
    assert all(t.id != task.id for t in tasks)


@pytest.mark.asyncio
async def test_delete_not_found():
    result = await db.delete_task(9999)
    assert result is False


@pytest.mark.asyncio
async def test_clear_done_removes_only_done():
    t1 = await db.add_task("done", "normal")
    t2 = await db.add_task("pending", "normal")
    await db.set_task_done(t1.id)
    count = await db.clear_done_tasks()
    assert count == 1
    remaining = await db.get_all_tasks()
    assert len(remaining) == 1
    assert remaining[0].id == t2.id


@pytest.mark.asyncio
async def test_time_display_formatted():
    task = await db.add_task("Test", "normal")
    # format: "HH:MM DD/MM"
    import re
    assert re.match(r"\d{2}:\d{2} \d{2}/\d{2}", task.time_display)
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cd backend && pytest tests/test_database.py -v
```

Expected: `ImportError` or `ModuleNotFoundError` for `database`.

- [ ] **Step 3: Write database.py**

```python
# backend/database.py
import aiosqlite
from datetime import datetime
from typing import Optional
from models import Task

_DB_PATH: str = ""


def _fmt(dt: datetime) -> str:
    return dt.strftime("%H:%M %d/%m")


async def init_db(path: str) -> None:
    global _DB_PATH
    _DB_PATH = path
    async with aiosqlite.connect(_DB_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS tasks (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                text         TEXT NOT NULL,
                priority     TEXT NOT NULL DEFAULT 'normal',
                status       TEXT NOT NULL DEFAULT 'pending',
                created_at   TEXT NOT NULL,
                time_display TEXT NOT NULL
            )
        """)
        await db.commit()


def _row_to_task(row: dict) -> Task:
    return Task(
        id=row["id"],
        text=row["text"],
        priority=row["priority"],
        status=row["status"],
        created_at=row["created_at"],
        time_display=row["time_display"],
    )


async def get_all_tasks() -> list[Task]:
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("""
            SELECT * FROM tasks
            ORDER BY
                CASE
                    WHEN status='pending' AND priority='urgent' THEN 0
                    WHEN status='pending' THEN 1
                    ELSE 2
                END,
                created_at ASC
        """) as cursor:
            rows = await cursor.fetchall()
            return [_row_to_task(dict(r)) for r in rows]


async def add_task(text: str, priority: str = "normal") -> Task:
    now = datetime.now()
    created_at = now.isoformat()
    time_display = _fmt(now)
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute(
            "INSERT INTO tasks (text, priority, created_at, time_display) VALUES (?, ?, ?, ?)",
            (text, priority, created_at, time_display),
        )
        await db.commit()
        task_id = cursor.lastrowid
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT * FROM tasks WHERE id=?", (task_id,)) as cursor:
            row = await cursor.fetchone()
            return _row_to_task(dict(row))


async def set_task_done(task_id: int) -> Optional[Task]:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute(
            "UPDATE tasks SET status='done' WHERE id=?", (task_id,)
        )
        await db.commit()
        if cursor.rowcount == 0:
            return None
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT * FROM tasks WHERE id=?", (task_id,)) as cursor:
            row = await cursor.fetchone()
            return _row_to_task(dict(row)) if row else None


async def delete_task(task_id: int) -> bool:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute("DELETE FROM tasks WHERE id=?", (task_id,))
        await db.commit()
        return cursor.rowcount > 0


async def clear_done_tasks() -> int:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute("DELETE FROM tasks WHERE status='done'")
        await db.commit()
        return cursor.rowcount
```

- [ ] **Step 4: Run tests — verify they pass**

```bash
cd backend && pytest tests/test_database.py -v
```

Expected: all 11 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/database.py backend/tests/test_database.py
git commit -m "feat: database layer with CRUD and sort order"
```

---

## Task 4: Auth Module

**Files:**
- Create: `backend/auth.py`
- Create: `backend/tests/test_auth.py`

- [ ] **Step 1: Write failing tests**

```python
# backend/tests/test_auth.py
import pytest
import os
from unittest.mock import MagicMock
from fastapi import HTTPException

os.environ.setdefault("API_KEY", "correct-key")
os.environ.setdefault("TELEGRAM_SECRET_TOKEN", "tg-secret")

from auth import verify_api_key, verify_telegram_token


@pytest.mark.asyncio
async def test_valid_api_key_passes():
    # Should not raise
    await verify_api_key(x_api_key="correct-key")


@pytest.mark.asyncio
async def test_wrong_api_key_raises_401():
    with pytest.raises(HTTPException) as exc_info:
        await verify_api_key(x_api_key="wrong-key")
    assert exc_info.value.status_code == 401


def test_valid_telegram_token():
    assert verify_telegram_token("tg-secret") is True


def test_invalid_telegram_token():
    assert verify_telegram_token("wrong") is False
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cd backend && pytest tests/test_auth.py -v
```

Expected: `ImportError` for `auth`.

- [ ] **Step 3: Write auth.py**

```python
# backend/auth.py
import hmac
import os
from fastapi import Header, HTTPException


async def verify_api_key(x_api_key: str = Header(...)) -> None:
    expected = os.getenv("API_KEY", "")
    if not hmac.compare_digest(x_api_key, expected):
        raise HTTPException(status_code=401, detail="Invalid API key")


def verify_telegram_token(token: str) -> bool:
    expected = os.getenv("TELEGRAM_SECRET_TOKEN", "")
    return hmac.compare_digest(token, expected)
```

- [ ] **Step 4: Run tests — verify they pass**

```bash
cd backend && pytest tests/test_auth.py -v
```

Expected: 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/auth.py backend/tests/test_auth.py
git commit -m "feat: API key and Telegram token auth"
```

---

## Task 5: Telegram Bot Handlers

**Files:**
- Create: `backend/bot.py`
- Create: `backend/tests/test_bot.py`

- [ ] **Step 1: Write failing tests**

```python
# backend/tests/test_bot.py
import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from telegram import Update, Message, Chat, User
from telegram.ext import ContextTypes
from models import Task
from datetime import datetime
import database as db
import bot


def _mock_task(id=1, text="Test", priority="normal", status="pending"):
    return Task(id, text, priority, status, datetime.now().isoformat(), "14:30 16/05")


def make_update(text: str, args: list[str] | None = None):
    message = MagicMock(spec=Message)
    message.text = text
    message.reply_text = AsyncMock()
    update = MagicMock(spec=Update)
    update.message = message
    context = MagicMock(spec=ContextTypes.DEFAULT_TYPE)
    context.args = args if args is not None else []
    return update, context


@pytest.mark.asyncio
async def test_plain_text_creates_normal_task():
    update, context = make_update("Comprar pines")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Comprar pines")) as mock_add:
        await bot.handle_text(update, context)
    mock_add.assert_called_once_with("Comprar pines", "normal")
    update.message.reply_text.assert_called_once()


@pytest.mark.asyncio
async def test_bang_creates_urgent_task():
    update, context = make_update("Arreglar cortocircuito!")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Arreglar cortocircuito", priority="urgent")) as mock_add:
        await bot.handle_text(update, context)
    mock_add.assert_called_once_with("Arreglar cortocircuito", "urgent")


@pytest.mark.asyncio
async def test_bang_stripped_from_text():
    update, context = make_update("Fix now!")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Fix now", priority="urgent")) as mock_add:
        await bot.handle_text(update, context)
    # stored text has no !
    call_text = mock_add.call_args[0][0]
    assert not call_text.endswith("!")


@pytest.mark.asyncio
async def test_cmd_add_normal():
    update, context = make_update("/add Soldar pines", args=["Soldar", "pines"])
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Soldar pines")) as mock_add:
        await bot.cmd_add(update, context)
    mock_add.assert_called_once_with("Soldar pines", "normal")


@pytest.mark.asyncio
async def test_cmd_add_urgent_flag():
    update, context = make_update("/add Fix this urgent", args=["Fix", "this", "urgent"])
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Fix this", priority="urgent")) as mock_add:
        await bot.cmd_add(update, context)
    mock_add.assert_called_once_with("Fix this", "urgent")


@pytest.mark.asyncio
async def test_cmd_add_no_args():
    update, context = make_update("/add", args=[])
    await bot.cmd_add(update, context)
    update.message.reply_text.assert_called_once()
    assert "Uso:" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_done_marks_task():
    update, context = make_update("/done 1", args=["1"])
    with patch.object(db, "set_task_done", new_callable=AsyncMock,
                      return_value=_mock_task(status="done")) as mock_done:
        await bot.cmd_done(update, context)
    mock_done.assert_called_once_with(1)
    update.message.reply_text.assert_called_once()


@pytest.mark.asyncio
async def test_cmd_done_not_found():
    update, context = make_update("/done 99", args=["99"])
    with patch.object(db, "set_task_done", new_callable=AsyncMock, return_value=None):
        await bot.cmd_done(update, context)
    assert "no encontrada" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_done_invalid_id():
    update, context = make_update("/done abc", args=["abc"])
    await bot.cmd_done(update, context)
    assert "inválido" in update.message.reply_text.call_args[0][0].lower()


@pytest.mark.asyncio
async def test_cmd_delete_found():
    update, context = make_update("/delete 1", args=["1"])
    with patch.object(db, "delete_task", new_callable=AsyncMock, return_value=True):
        await bot.cmd_delete(update, context)
    assert "#1" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_list_empty():
    update, context = make_update("/list")
    with patch.object(db, "get_all_tasks", new_callable=AsyncMock, return_value=[]):
        await bot.cmd_list(update, context)
    assert "Sin tareas" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_clear():
    update, context = make_update("/clear")
    with patch.object(db, "clear_done_tasks", new_callable=AsyncMock, return_value=2):
        await bot.cmd_clear(update, context)
    assert "2" in update.message.reply_text.call_args[0][0]
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cd backend && pytest tests/test_bot.py -v
```

Expected: `ImportError` for `bot`.

- [ ] **Step 3: Write bot.py**

```python
# backend/bot.py
import os
from telegram import Update
from telegram.ext import (
    Application, CommandHandler, MessageHandler,
    filters, ContextTypes,
)
import database as db


def build_application() -> Application:
    app = Application.builder().token(os.getenv("TELEGRAM_BOT_TOKEN", "")).build()
    app.add_handler(CommandHandler("start", cmd_start))
    app.add_handler(CommandHandler("add", cmd_add))
    app.add_handler(CommandHandler("done", cmd_done))
    app.add_handler(CommandHandler("delete", cmd_delete))
    app.add_handler(CommandHandler("list", cmd_list))
    app.add_handler(CommandHandler("clear", cmd_clear))
    app.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, handle_text))
    return app


async def cmd_start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(
        "ESP32 Task Panel\n\n"
        "Envía texto → tarea normal\n"
        "Envía texto! → tarea urgente\n\n"
        "/add <texto> [urgent]\n"
        "/done <id>\n"
        "/delete <id>\n"
        "/list\n"
        "/clear"
    )


async def handle_text(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    text = update.message.text.strip()
    if text.endswith("!"):
        priority = "urgent"
        text = text[:-1].strip()
    else:
        priority = "normal"
    task = await db.add_task(text, priority)
    label = "URGENTE" if priority == "urgent" else "normal"
    await update.message.reply_text(f"✅ #{task.id} [{label}] {task.text}")


async def cmd_add(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /add <texto> [urgent]")
        return
    args = list(context.args)
    priority = "normal"
    if args[-1].lower() == "urgent":
        priority = "urgent"
        args = args[:-1]
    if not args:
        await update.message.reply_text("Uso: /add <texto> [urgent]")
        return
    task = await db.add_task(" ".join(args), priority)
    label = "URGENTE" if priority == "urgent" else "normal"
    await update.message.reply_text(f"✅ #{task.id} [{label}] {task.text}")


async def cmd_done(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /done <id>")
        return
    try:
        task_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("ID inválido")
        return
    task = await db.set_task_done(task_id)
    if task:
        await update.message.reply_text(f"✔️ #{task.id} completada")
    else:
        await update.message.reply_text(f"Tarea #{task_id} no encontrada")


async def cmd_delete(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /delete <id>")
        return
    try:
        task_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("ID inválido")
        return
    deleted = await db.delete_task(task_id)
    if deleted:
        await update.message.reply_text(f"🗑️ #{task_id} eliminada")
    else:
        await update.message.reply_text(f"Tarea #{task_id} no encontrada")


async def cmd_list(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    tasks = await db.get_all_tasks()
    if not tasks:
        await update.message.reply_text("Sin tareas")
        return
    lines = []
    for t in tasks:
        icon = "✔️" if t.status == "done" else ("🔴" if t.priority == "urgent" else "⏳")
        lines.append(f"{icon} #{t.id} {t.text} — {t.time_display}")
    await update.message.reply_text("\n".join(lines))


async def cmd_clear(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    count = await db.clear_done_tasks()
    await update.message.reply_text(f"🗑️ {count} tarea(s) done eliminadas")
```

- [ ] **Step 4: Run tests — verify they pass**

```bash
cd backend && pytest tests/test_bot.py -v
```

Expected: 12 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/bot.py backend/tests/test_bot.py
git commit -m "feat: telegram bot handlers with plain-text and urgent support"
```

---

## Task 6: FastAPI App + Webhook

**Files:**
- Create: `backend/main.py`
- Create: `backend/tests/test_api.py`

- [ ] **Step 1: Write failing tests**

```python
# backend/tests/test_api.py
import pytest
import os
from unittest.mock import AsyncMock, patch, MagicMock

os.environ.setdefault("API_KEY", "test-key")
os.environ.setdefault("TELEGRAM_BOT_TOKEN", "123:test")
os.environ.setdefault("TELEGRAM_SECRET_TOKEN", "tg-secret")
os.environ.setdefault("DATABASE_URL", ":memory:")
os.environ.setdefault("BASE_URL", "https://example.com")

from httpx import AsyncClient, ASGITransport
from models import Task
from datetime import datetime

MOCK_TASKS = [
    Task(1, "Buy parts", "urgent", "pending", datetime.now().isoformat(), "14:30 16/05"),
    Task(2, "Solder pins", "normal", "done", datetime.now().isoformat(), "10:00 16/05"),
]


@pytest.fixture
async def client():
    # Patch bot setup so we don't need a real Telegram token
    with patch("main.build_application") as mock_build:
        mock_app = MagicMock()
        mock_app.initialize = AsyncMock()
        mock_app.bot.set_webhook = AsyncMock()
        mock_app.shutdown = AsyncMock()
        mock_app.process_update = AsyncMock()
        mock_build.return_value = mock_app

        from main import app
        async with AsyncClient(
            transport=ASGITransport(app=app), base_url="http://test"
        ) as c:
            yield c


@pytest.mark.asyncio
async def test_health(client):
    resp = await client.get("/health")
    assert resp.status_code == 200
    assert resp.json() == {"status": "ok"}


@pytest.mark.asyncio
async def test_tasks_missing_key(client):
    resp = await client.get("/tasks")
    assert resp.status_code == 422


@pytest.mark.asyncio
async def test_tasks_wrong_key(client):
    resp = await client.get("/tasks", headers={"X-API-Key": "wrong"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_tasks_returns_list(client):
    with patch("main.get_all_tasks", new_callable=AsyncMock, return_value=MOCK_TASKS):
        resp = await client.get("/tasks", headers={"X-API-Key": "test-key"})
    assert resp.status_code == 200
    data = resp.json()
    assert len(data) == 2
    assert data[0]["text"] == "Buy parts"
    assert "time_display" in data[0]
    assert "created_at" not in data[0]   # to_dict() excludes it


@pytest.mark.asyncio
async def test_webhook_wrong_token(client):
    resp = await client.post(
        "/webhook", json={},
        headers={"X-Telegram-Bot-Api-Secret-Token": "wrong"}
    )
    assert resp.status_code == 403


@pytest.mark.asyncio
async def test_webhook_valid_token(client):
    with patch("main.Update.de_json", return_value=MagicMock()):
        resp = await client.post(
            "/webhook", json={"update_id": 1},
            headers={"X-Telegram-Bot-Api-Secret-Token": "tg-secret"}
        )
    assert resp.status_code == 200
    assert resp.json() == {"ok": True}
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cd backend && pytest tests/test_api.py -v
```

Expected: `ImportError` for `main`.

- [ ] **Step 3: Write main.py**

```python
# backend/main.py
import os
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request, HTTPException
from dotenv import load_dotenv
from telegram import Update

load_dotenv()

from database import init_db, get_all_tasks
from auth import verify_api_key, verify_telegram_token
from bot import build_application


@asynccontextmanager
async def lifespan(app: FastAPI):
    await init_db(os.getenv("DATABASE_URL", "./tasks.db"))
    bot_app = build_application()
    await bot_app.initialize()
    await bot_app.bot.set_webhook(
        url=f"{os.getenv('BASE_URL')}/webhook",
        secret_token=os.getenv("TELEGRAM_SECRET_TOKEN"),
    )
    app.state.bot_app = bot_app
    yield
    await bot_app.shutdown()


app = FastAPI(lifespan=lifespan)


@app.get("/health")
async def health():
    return {"status": "ok"}


@app.get("/tasks")
async def tasks_endpoint(_=Depends(verify_api_key)):
    tasks = await get_all_tasks()
    return [t.to_dict() for t in tasks]


@app.post("/webhook")
async def webhook(request: Request):
    token = request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")
    if not verify_telegram_token(token):
        raise HTTPException(status_code=403, detail="Forbidden")
    data = await request.json()
    update = Update.de_json(data, app.state.bot_app.bot)
    await app.state.bot_app.process_update(update)
    return {"ok": True}
```

- [ ] **Step 4: Run all tests**

```bash
cd backend && pytest -v
```

Expected: all tests PASS (database + auth + bot + api).

- [ ] **Step 5: Commit**

```bash
git add backend/main.py backend/tests/test_api.py
git commit -m "feat: fastapi app with /tasks, /webhook, /health endpoints"
```

---

## Task 7: OCI Deployment

**Files:**
- Create: `backend/esp32-tasks.service`
- Create: `backend/.env` (on server only — gitignored)

> **Prerequisites:** OCI instance running Ubuntu 22.04, domain pointing to its IP, port 443 open in OCI security list.

- [ ] **Step 1: Create systemd service file**

```ini
# backend/esp32-tasks.service
[Unit]
Description=ESP32 Task Panel Backend
After=network.target

[Service]
User=ubuntu
WorkingDirectory=/home/ubuntu/esp32-task-panel/backend
EnvironmentFile=/home/ubuntu/esp32-task-panel/backend/.env
ExecStart=/home/ubuntu/esp32-task-panel/venv/bin/uvicorn main:app --host 127.0.0.1 --port 8000
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 2: On OCI server — install dependencies**

```bash
sudo apt update && sudo apt install -y python3.11 python3.11-venv nginx certbot python3-certbot-nginx git
```

- [ ] **Step 3: Clone repo and setup venv on server**

```bash
git clone <your-repo-url> ~/esp32-task-panel
cd ~/esp32-task-panel/backend
python3.11 -m venv venv
venv/bin/pip install -r requirements.txt
```

- [ ] **Step 4: Create .env on server**

```bash
cp .env.example .env
nano .env   # fill in all values
```

Generate random tokens:
```bash
python3 -c "import secrets; print(secrets.token_hex(16))"
```

- [ ] **Step 5: Setup Nginx reverse proxy**

```bash
sudo nano /etc/nginx/sites-available/esp32-tasks
```

```nginx
server {
    listen 80;
    server_name your-oci-domain.com;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/esp32-tasks /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

- [ ] **Step 6: Get HTTPS cert**

```bash
sudo certbot --nginx -d your-oci-domain.com
```

Follow prompts. Certbot auto-renews.

- [ ] **Step 7: Enable and start service**

```bash
sudo cp ~/esp32-task-panel/backend/esp32-tasks.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable esp32-tasks
sudo systemctl start esp32-tasks
sudo systemctl status esp32-tasks
```

Expected: `active (running)`.

- [ ] **Step 8: Verify endpoints**

```bash
curl https://your-oci-domain.com/health
# Expected: {"status":"ok"}

curl -H "X-API-Key: your_api_key" https://your-oci-domain.com/tasks
# Expected: []
```

- [ ] **Step 9: Register Telegram webhook**

Get your bot token from BotFather, then:
```bash
curl -X POST "https://api.telegram.org/bot<TOKEN>/setWebhook" \
  -H "Content-Type: application/json" \
  -d '{"url":"https://your-oci-domain.com/webhook","secret_token":"<TELEGRAM_SECRET_TOKEN>"}'
```

Expected: `{"ok":true,"result":true}`

- [ ] **Step 10: Test bot end-to-end**

Send `/start` to your bot in Telegram. Expected: help message.
Send `Comprar resistencias!` in Telegram. Expected: `✅ #1 [URGENTE] Comprar resistencias`.
```bash
curl -H "X-API-Key: your_api_key" https://your-oci-domain.com/tasks
```
Expected: task appears in response JSON.

- [ ] **Step 11: Commit service file**

```bash
git add backend/esp32-tasks.service
git commit -m "chore: OCI systemd service config"
```

---

## Task 8: ESP32 — Non-Sensitive Config

**Files:**
- Create: `firmware/src/config.h`

- [ ] **Step 1: Write config.h**

```cpp
// firmware/src/config.h
#pragma once

#define POLL_INTERVAL_MS    5000UL
#define NTP_SYNC_INTERVAL   3600000UL
#define NTP_SERVER          "pool.ntp.org"
#define MAX_TASKS           8

// Display geometry (320x240 landscape)
#define SCREEN_W    320
#define SCREEN_H    240
#define HEADER_H     28
#define TASK_H       38
#define TASK_MARGIN   3
#define BAR_W         3
```

- [ ] **Step 2: Copy secrets.h.example to secrets.h and fill in**

```bash
cp firmware/src/secrets.h.example firmware/src/secrets.h
# Edit secrets.h with your values — this file is gitignored
```

- [ ] **Step 3: Commit config.h**

```bash
git add firmware/src/config.h
git commit -m "feat: esp32 non-sensitive config"
```

---

## Task 9: ESP32 — Task Model + SPIFFS Cache

**Files:**
- Create: `firmware/src/tasks.h`
- Create: `firmware/src/tasks.cpp`

- [ ] **Step 1: Write tasks.h**

```cpp
// firmware/src/tasks.h
#pragma once
#include <Arduino.h>
#include "config.h"

struct TaskItem {
    int  id;
    char text[64];
    char priority[8];     // "normal" | "urgent"
    char status[8];       // "pending" | "done"
    char time_display[16]; // "14:30 16/05"
};

// Sort: urgent-pending first, normal-pending second, done last
void sortTasks(TaskItem* tasks, int count);

// Returns true on success
bool saveTasks(const TaskItem* tasks, int count);

// Returns number of tasks loaded (0 if file missing/corrupt)
int  loadTasks(TaskItem* tasks);
```

- [ ] **Step 2: Write tasks.cpp**

```cpp
// firmware/src/tasks.cpp
#include "tasks.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

static int scoreTask(const TaskItem& t) {
    if (strcmp(t.status, "done") == 0)    return 2;
    if (strcmp(t.priority, "urgent") == 0) return 0;
    return 1;
}

void sortTasks(TaskItem* tasks, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (scoreTask(tasks[j]) < scoreTask(tasks[i])) {
                TaskItem tmp = tasks[i];
                tasks[i]     = tasks[j];
                tasks[j]     = tmp;
            }
        }
    }
}

bool saveTasks(const TaskItem* tasks, int count) {
    File f = SPIFFS.open("/tasks.json", "w");
    if (!f) return false;

    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["id"]           = tasks[i].id;
        obj["text"]         = tasks[i].text;
        obj["priority"]     = tasks[i].priority;
        obj["status"]       = tasks[i].status;
        obj["time_display"] = tasks[i].time_display;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

int loadTasks(TaskItem* tasks) {
    if (!SPIFFS.exists("/tasks.json")) return 0;

    File f = SPIFFS.open("/tasks.json", "r");
    if (!f) return 0;

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return 0;

    JsonArray arr = doc.as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        if (count >= MAX_TASKS) break;
        tasks[count].id = obj["id"] | 0;
        strlcpy(tasks[count].text,         obj["text"]         | "", sizeof(tasks[count].text));
        strlcpy(tasks[count].priority,     obj["priority"]     | "normal", sizeof(tasks[count].priority));
        strlcpy(tasks[count].status,       obj["status"]       | "pending", sizeof(tasks[count].status));
        strlcpy(tasks[count].time_display, obj["time_display"] | "", sizeof(tasks[count].time_display));
        count++;
    }
    return count;
}
```

- [ ] **Step 3: Build to verify compilation**

```bash
cd firmware && pio run
```

Expected: SUCCESS (no errors referencing tasks.h/tasks.cpp).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/tasks.h firmware/src/tasks.cpp
git commit -m "feat: esp32 task model and SPIFFS cache"
```

---

## Task 10: ESP32 — Network Module (WiFi + HTTPS + NTP)

**Files:**
- Create: `firmware/src/network.h`
- Create: `firmware/src/network.cpp`

- [ ] **Step 1: Write network.h**

```cpp
// firmware/src/network.h
#pragma once
#include <Arduino.h>
#include "tasks.h"

// Returns true if connected
bool connectWiFi();

// Syncs NTP. Returns true on success.
bool syncNTP();

// Returns current time as "HH:MM". Call after syncNTP().
String getCurrentTime();

// Fetches tasks from API. Returns task count, -1 on error.
int fetchTasks(TaskItem* tasks);
```

- [ ] **Step 2: Write network.cpp**

```cpp
// firmware/src/network.cpp
#include "network.h"
#include "secrets.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

static WiFiUDP    _ntpUDP;
static NTPClient  _timeClient(_ntpUDP, NTP_SERVER, TZ_OFFSET * 3600, 60000);

bool connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool syncNTP() {
    _timeClient.begin();
    return _timeClient.forceUpdate();
}

String getCurrentTime() {
    _timeClient.update();
    // getFormattedTime() returns "HH:MM:SS"
    return _timeClient.getFormattedTime().substring(0, 5);
}

int fetchTasks(TaskItem* tasks) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    WiFiClientSecure client;
    client.setInsecure(); // accepts any cert — sufficient for private OCI endpoint

    HTTPClient http;
    http.begin(client, API_URL);
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return -1;

    JsonArray arr = doc.as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        if (count >= MAX_TASKS) break;
        tasks[count].id = obj["id"] | 0;
        strlcpy(tasks[count].text,         obj["text"]         | "", sizeof(tasks[count].text));
        strlcpy(tasks[count].priority,     obj["priority"]     | "normal", sizeof(tasks[count].priority));
        strlcpy(tasks[count].status,       obj["status"]       | "pending", sizeof(tasks[count].status));
        strlcpy(tasks[count].time_display, obj["time_display"] | "", sizeof(tasks[count].time_display));
        count++;
    }
    return count;
}
```

- [ ] **Step 3: Build to verify compilation**

```bash
cd firmware && pio run
```

Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/network.h firmware/src/network.cpp
git commit -m "feat: esp32 network module — wifi, https polling, ntp"
```

---

## Task 11: ESP32 — Display Module

**Files:**
- Create: `firmware/src/display.h`
- Create: `firmware/src/display.cpp`

- [ ] **Step 1: Write display.h**

```cpp
// firmware/src/display.h
#pragma once
#include <TFT_eSPI.h>
#include "tasks.h"
#include "config.h"

extern TFT_eSPI tft;

void displayInit();
void drawSplash(const String& msg);

// Redraws full screen: header + all task cards
void redrawAll(TaskItem* tasks, int count, const String& time_str, bool online);
```

- [ ] **Step 2: Write display.cpp**

```cpp
// firmware/src/display.cpp
#include "display.h"

TFT_eSPI tft = TFT_eSPI();

// Colors
#define C_BG       0x0000  // black
#define C_HEADER   0x2104  // dark grey
#define C_CARD     0x0841  // very dark grey
#define C_DONE_BG  0x0421  // near-black for done cards

void displayInit() {
    tft.init();
    tft.setRotation(1);  // landscape
    tft.fillScreen(C_BG);
}

void drawSplash(const String& msg) {
    tft.fillScreen(C_BG);
    tft.setTextColor(TFT_CYAN, C_BG);
    tft.drawCentreString("ESP32 TASKS", SCREEN_W / 2, 90, 4);
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString(msg, SCREEN_W / 2, 130, 2);
}

static void drawHeader(const String& time_str, int pending, bool online) {
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);

    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.drawString("TAREAS", 8, 7, 2);

    uint16_t timeColor = online ? TFT_GREEN : TFT_RED;
    tft.setTextColor(timeColor, C_HEADER);
    tft.drawCentreString(online ? time_str : "OFFLINE", SCREEN_W / 2, 7, 2);

    tft.setTextColor(TFT_DARKGREY, C_HEADER);
    tft.drawRightString(String(pending) + " pend", SCREEN_W - 6, 7, 2);
}

static void drawTask(int index, const TaskItem& task) {
    int y = HEADER_H + TASK_MARGIN + index * (TASK_H + TASK_MARGIN);
    if (y + TASK_H > SCREEN_H - 10) return;

    bool done   = strcmp(task.status, "done") == 0;
    bool urgent = strcmp(task.priority, "urgent") == 0;

    uint16_t barColor  = done ? TFT_DARKGREY : (urgent ? TFT_RED : TFT_GREEN);
    uint16_t cardBg    = done ? C_DONE_BG : C_CARD;
    uint16_t textColor = done ? TFT_DARKGREY : TFT_WHITE;
    uint16_t metaColor = done ? 0x2945 : 0x4228;

    tft.fillRect(0, y, SCREEN_W, TASK_H, cardBg);
    tft.fillRect(0, y, BAR_W, TASK_H, barColor);

    // Truncate text to fit (font 2 ≈ 12px per char, usable width ~260px → ~21 chars)
    String txt = String(task.text);
    if (txt.length() > 26) txt = txt.substring(0, 25) + "~";

    tft.setTextColor(textColor, cardBg);
    tft.drawString(txt, BAR_W + 6, y + 5, 2);

    tft.setTextColor(metaColor, cardBg);
    tft.drawString(String(task.time_display), BAR_W + 6, y + 22, 1);

    const char* badge = done ? "DONE" : (urgent ? "URGENTE" : "PEND");
    tft.setTextColor(barColor, cardBg);
    tft.drawRightString(badge, SCREEN_W - 6, y + 13, 1);
}

static void drawNoTasks() {
    tft.setTextColor(TFT_DARKGREY, C_BG);
    tft.drawCentreString("Sin tareas", SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
    tft.drawCentreString("Envia texto al bot de Telegram", SCREEN_W / 2, SCREEN_H / 2 + 10, 1);
}

void redrawAll(TaskItem* tasks, int count, const String& time_str, bool online) {
    int pending = 0;
    for (int i = 0; i < count; i++)
        if (strcmp(tasks[i].status, "pending") == 0) pending++;

    drawHeader(time_str, pending, online);
    tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, C_BG);

    if (count == 0) {
        drawNoTasks();
        return;
    }

    int maxVisible = (SCREEN_H - HEADER_H - TASK_MARGIN) / (TASK_H + TASK_MARGIN);
    int visible    = min(count, maxVisible);

    for (int i = 0; i < visible; i++) {
        drawTask(i, tasks[i]);
    }

    if (count > maxVisible) {
        tft.setTextColor(TFT_DARKGREY, C_BG);
        tft.drawCentreString(
            "+" + String(count - maxVisible) + " mas",
            SCREEN_W / 2, SCREEN_H - 10, 1
        );
    }
}
```

- [ ] **Step 3: Build to verify compilation**

```bash
cd firmware && pio run
```

Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat: esp32 display module — minimal layout with priority bars"
```

---

## Task 12: ESP32 — main.ino Integration

**Files:**
- Create: `firmware/src/main.ino`

- [ ] **Step 1: Write main.ino**

```cpp
// firmware/src/main.ino
#include "config.h"
#include "secrets.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include <SPIFFS.h>

static TaskItem tasks[MAX_TASKS];
static int      taskCount  = 0;
static bool     online     = false;
static unsigned long lastPoll = 0;
static unsigned long lastNTP  = 0;

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
    }

    String t = online ? getCurrentTime() : "--:--";
    redrawAll(tasks, taskCount, t, online);
}

void loop() {
    unsigned long now = millis();

    if (online && (now - lastNTP >= NTP_SYNC_INTERVAL)) {
        syncNTP();
        lastNTP = now;
    }

    if (now - lastPoll >= POLL_INTERVAL_MS) {
        int result = fetchTasks(tasks);
        if (result >= 0) {
            taskCount = result;
            online    = true;
            sortTasks(tasks, taskCount);
            saveTasks(tasks, taskCount);
        } else {
            online = false;
            // keep existing taskCount from SPIFFS
        }
        String t = online ? getCurrentTime() : "--:--";
        redrawAll(tasks, taskCount, t, online);
        lastPoll = now;
    }
}
```

- [ ] **Step 2: Build firmware**

```bash
cd firmware && pio run
```

Expected: SUCCESS, binary size shown (should be well under 4MB flash limit).

- [ ] **Step 3: Flash to ESP32**

```bash
cd firmware && pio run --target upload
```

Expected: upload completes, ESP32 reboots.

- [ ] **Step 4: Monitor serial output**

```bash
cd firmware && pio device monitor
```

Expected output sequence:
```
WiFi connected
NTP synced
Fetched 0 tasks
```

- [ ] **Step 5: End-to-end test**

1. Send `Comprar resistencias!` to Telegram bot
2. Expected Telegram reply: `✅ #1 [URGENTE] Comprar resistencias`
3. Wait ≤5 seconds
4. Expected on TFT: red bar card with "Comprar resistencias" + "URGENTE" badge
5. Send `/done 1` to Telegram
6. Wait ≤5 seconds
7. Expected: card turns grey/dimmed with "DONE" badge

- [ ] **Step 6: Test offline resilience**

1. Power off router briefly
2. Expected: TFT header shows "OFFLINE" in red
3. Tasks from SPIFFS still visible on screen
4. Restore WiFi → header turns green, resumes polling

- [ ] **Step 7: Commit**

```bash
git add firmware/src/main.ino
git commit -m "feat: esp32 main loop — boot sequence, poll, offline resilience"
```

---

## Done Criteria

- [ ] `pytest` in `backend/` — all tests green
- [ ] `/health` returns `{"status":"ok"}` on OCI
- [ ] Telegram plain text and commands create/update tasks in SQLite
- [ ] `/tasks` returns correct sorted JSON (urgent first, done last)
- [ ] ESP32 displays tasks within 5s of Telegram message
- [ ] ESP32 shows OFFLINE indicator when API unreachable
- [ ] Tasks survive ESP32 reboot (SPIFFS) and backend restart (SQLite)
- [ ] `secrets.h` and `.env` not in git history
