import time

import httpx
import pytest

import spotify


@pytest.fixture(autouse=True)
def _reset(monkeypatch):
    for k in ("SPOTIFY_CLIENT_ID", "SPOTIFY_CLIENT_SECRET", "SPOTIFY_REFRESH_TOKEN"):
        monkeypatch.setenv(k, "x")
    monkeypatch.setattr(spotify, "_backoff_until", 0.0)
    monkeypatch.setattr(spotify, "_access_token", "tok")
    monkeypatch.setattr(spotify, "_token_expires_at", time.time() + 1000)


class FakeResp:
    def __init__(self, status=200, body=None, headers=None):
        self.status_code, self._body, self.headers, self.text = status, body, headers or {}, ""

    def json(self):
        return self._body


def patch_client(monkeypatch, resp=None, exc=None):
    calls = []

    class FakeClient:
        def __init__(self, *a, **k): pass
        async def __aenter__(self): return self
        async def __aexit__(self, *a): return False
        async def get(self, *a, **k):
            calls.append(1)
            if exc: raise exc
            return resp
        post = get

    monkeypatch.setattr(spotify.httpx, "AsyncClient", FakeClient)
    return calls


async def test_missing_env_no_http(monkeypatch):
    monkeypatch.setenv("SPOTIFY_CLIENT_ID", "")
    calls = patch_client(monkeypatch, FakeResp())
    assert await spotify.get_now_playing() == {"is_playing": False}
    assert calls == []


async def test_null_item(monkeypatch):
    patch_client(monkeypatch, FakeResp(200, {"is_playing": True, "item": None}))
    assert await spotify.get_now_playing() == {"is_playing": False}


async def test_null_image_width():
    assert spotify._pick_artwork_url([{"url": "u", "width": None}]) == "u"


async def test_http_error_and_backoff(monkeypatch):
    calls = patch_client(monkeypatch, exc=httpx.ConnectError("boom"))
    assert await spotify.get_now_playing() == {"is_playing": False}
    assert await spotify.get_now_playing() == {"is_playing": False}
    assert len(calls) == 1  # second call short-circuited by backoff


async def test_429_retry_after(monkeypatch):
    patch_client(monkeypatch, FakeResp(429, headers={"Retry-After": "5"}))
    assert await spotify.get_now_playing() == {"is_playing": False}
    assert 0 < spotify._backoff_until - time.time() <= 5
