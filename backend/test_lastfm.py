import io
import time

import httpx
import pytest
from PIL import Image

import lastfm


def png_bytes(size=300):
    out = io.BytesIO()
    Image.new("RGB", (size, size), (200, 30, 90)).save(out, "PNG")
    return out.getvalue()


@pytest.fixture(autouse=True)
def _reset(monkeypatch):
    monkeypatch.setenv("LASTFM_API_KEY", "k")
    monkeypatch.setenv("LASTFM_USER", "u")
    monkeypatch.setenv("BASE_URL", "https://example.com")
    monkeypatch.setattr(lastfm, "_extra", {})
    for name, val in (("_cache", None), ("_cache_at", 0.0), ("_backoff_until", 0.0),
                      ("_np_key", None), ("_np_since", 0.0)):
        monkeypatch.setattr(lastfm, name, val)


class FakeResp:
    def __init__(self, body=None, content=b"", status=200):
        self._body, self.content, self.status_code = body, content, status

    def json(self):
        return self._body

    def raise_for_status(self):
        if self.status_code >= 400:
            raise httpx.HTTPStatusError("err", request=None, response=None)


def patch_client(monkeypatch, api_body=None, exc=None, info=None, itunes=None):
    calls = []

    class FakeClient:
        def __init__(self, *a, **k): pass
        async def __aenter__(self): return self
        async def __aexit__(self, *a): return False
        async def get(self, url, **k):
            calls.append(url)
            if exc: raise exc
            if url == lastfm.API_URL:
                if (k.get("params") or {}).get("method") == "track.getInfo":
                    return FakeResp(info or {})
                return FakeResp(api_body)
            if url == lastfm.ITUNES_URL:
                return FakeResp(itunes or {"results": []})
            return FakeResp(content=png_bytes())

    monkeypatch.setattr(lastfm.httpx, "AsyncClient", FakeClient)
    return calls


def track(nowplaying=True):
    t = {"name": "Song", "artist": {"#text": "Band"}, "album": {"#text": "Album"},
         "image": [{"size": "small", "#text": "https://img/34.png"},
                   {"size": "medium", "#text": "https://img/64.png"},
                   {"size": "extralarge", "#text": "https://img/300.png"}]}
    if nowplaying:
        t["@attr"] = {"nowplaying": "true"}
    return {"recenttracks": {"track": [t]}}


async def test_not_configured_no_http(monkeypatch):
    monkeypatch.setenv("LASTFM_USER", "")
    calls = patch_client(monkeypatch, track())
    assert await lastfm.get_now_playing() == {"is_playing": False}
    assert calls == []


async def test_nowplaying_track(monkeypatch):
    patch_client(monkeypatch, track())
    np = await lastfm.get_now_playing()
    assert np["is_playing"] and np["title"] == "Song" and np["artist"] == "Band"
    assert np["album"] == "Album" and np["duration_ms"] == 0
    assert np["artwork_url"] == "https://img/300.png"   # largest; proxied later


async def test_last_scrobble_is_not_nowplaying(monkeypatch):
    patch_client(monkeypatch, track(nowplaying=False))
    assert await lastfm.get_now_playing() == {"is_playing": False}


async def test_single_track_dict_and_cache(monkeypatch):
    body = track()
    body["recenttracks"]["track"] = body["recenttracks"]["track"][0]  # API quirk
    calls = patch_client(monkeypatch, body)
    assert (await lastfm.get_now_playing())["is_playing"]
    n = len(calls)
    assert (await lastfm.get_now_playing())["is_playing"]
    assert len(calls) == n  # served from cache


async def test_error_backs_off(monkeypatch):
    calls = patch_client(monkeypatch, exc=httpx.ConnectError("boom"))
    assert await lastfm.get_now_playing() == {"is_playing": False}
    monkeypatch.setattr(lastfm, "_cache_at", 0.0)
    assert await lastfm.get_now_playing() == {"is_playing": False}
    assert len(calls) == 1
    assert lastfm._backoff_until > time.time()



async def test_duration_and_progress(monkeypatch):
    patch_client(monkeypatch, track(), info={"track": {"duration": "200000"}})
    monkeypatch.setattr(lastfm.time, "time", lambda: 1000.0)
    np = await lastfm.get_now_playing()
    assert np["duration_ms"] == 200000 and np["progress_ms"] == 0
    monkeypatch.setattr(lastfm.time, "time", lambda: 1030.0)
    np = await lastfm.get_now_playing()
    assert np["progress_ms"] == 30000
    monkeypatch.setattr(lastfm.time, "time", lambda: 2000.0)
    np = await lastfm.get_now_playing()
    assert np["progress_ms"] == 200000          # clamped to the duration


async def test_placeholder_cover_uses_itunes(monkeypatch):
    body = track()
    for img in body["recenttracks"]["track"][0]["image"]:
        img["#text"] = "https://lastfm/i/u/300x300/2a96cbd8b46e442fc41c2b86b821562f.png"
    patch_client(monkeypatch, body, itunes={"results": [
        {"artworkUrl100": "https://itunes/a/100x100bb.jpg", "trackTimeMillis": 180000}]})
    np = await lastfm.get_now_playing()
    assert np["artwork_url"] == "https://itunes/a/300x300bb.jpg"
    assert np["duration_ms"] == 180000
