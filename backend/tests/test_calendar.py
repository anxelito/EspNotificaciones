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
async def test_calendar_returns_empty_list(client):
    with patch("main.fetch_events_async", new_callable=AsyncMock, return_value=[]):
        resp = await client.get("/calendar", headers={"X-API-Key": "test-key"})
    assert resp.status_code == 200
    assert resp.json() == []
