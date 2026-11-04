import httpx
import pytest

import weather


@pytest.fixture(autouse=True)
def _reset(monkeypatch):
    monkeypatch.setenv("WEATHER_LAT", "43.36")
    monkeypatch.setenv("WEATHER_LON", "-8.41")
    monkeypatch.setattr(weather, "_cache", None)
    monkeypatch.setattr(weather, "_cache_at", 0.0)


def patch_client(monkeypatch, body=None, exc=None):
    calls = []

    class Resp:
        def raise_for_status(self): pass
        def json(self): return body

    class FakeClient:
        def __init__(self, *a, **k): pass
        async def __aenter__(self): return self
        async def __aexit__(self, *a): return False
        async def get(self, url, **k):
            calls.append(k.get("params"))
            if exc: raise exc
            return Resp()

    monkeypatch.setattr(weather.httpx, "AsyncClient", FakeClient)
    return calls


BODY = {"current": {"temperature_2m": 14.2, "weather_code": 61, "is_day": 1}}


async def test_not_configured(monkeypatch):
    monkeypatch.setenv("WEATHER_LAT", "")
    calls = patch_client(monkeypatch, BODY)
    assert await weather.get_weather() == {"ok": False}
    assert calls == []


async def test_reads_current_and_caches(monkeypatch):
    calls = patch_client(monkeypatch, BODY)
    w = await weather.get_weather()
    assert w == {"ok": True, "temp": 14.2, "code": 61, "is_day": True}
    assert calls[0]["latitude"] == "43.36"
    await weather.get_weather()
    assert len(calls) == 1


async def test_error_keeps_last_good(monkeypatch):
    patch_client(monkeypatch, BODY)
    good = await weather.get_weather()
    monkeypatch.setattr(weather, "_cache_at", 0.0)          # expire
    patch_client(monkeypatch, exc=httpx.ConnectError("boom"))
    assert await weather.get_weather() == good
