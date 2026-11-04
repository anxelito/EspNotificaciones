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
    with patch("main.build_application") as mock_build:
        mock_app = MagicMock()
        mock_app.initialize = AsyncMock()
        mock_app.bot.set_webhook = AsyncMock()
        mock_app.shutdown = AsyncMock()
        mock_app.process_update = AsyncMock()
        mock_build.return_value = mock_app

        from main import app
        app.state.bot_app = mock_app  # lifespan doesn't run in ASGITransport
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
    assert "created_at" not in data[0]


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



# --- Now playing source order --------------------------------------------------
KEY = {"X-API-Key": "test-key"}
LF = {"is_playing": True, "title": "LF song", "artist": "B", "album": "",
      "artwork_url": "", "progress_ms": 0, "duration_ms": 0}


@pytest.mark.asyncio
async def test_nowplaying_falls_back_to_lastfm(client):
    with patch("main.get_now_playing", AsyncMock(return_value={"is_playing": False})),          patch("main.lastfm.get_now_playing", AsyncMock(return_value=LF)):
        np = (await client.get("/nowplaying", headers=KEY)).json()
    assert np["title"] == "LF song" and np["duration_ms"] == 0


@pytest.mark.asyncio
async def test_nowplaying_prefers_spotify_api(client):
    sp = {"is_playing": True, "title": "API song", "artist": "A", "album": "B",
          "artwork_url": "", "progress_ms": 1000, "duration_ms": 2000}
    lf = AsyncMock(return_value=LF)
    with patch("main.get_now_playing", AsyncMock(return_value=sp)),          patch("main.lastfm.get_now_playing", lf):
        np = (await client.get("/nowplaying", headers=KEY)).json()
    assert np["title"] == "API song" and np["duration_ms"] == 2000
    lf.assert_not_called()


@pytest.mark.asyncio
async def test_nowplaying_nothing(client):
    with patch("main.get_now_playing", AsyncMock(return_value={"is_playing": False})),          patch("main.lastfm.get_now_playing", AsyncMock(return_value={"is_playing": False})):
        np = (await client.get("/nowplaying", headers=KEY)).json()
    assert np == {"is_playing": False}
