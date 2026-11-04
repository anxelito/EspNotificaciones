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
    for name, val in (("_cache", None), ("_cache_at", 0.0), ("_backoff_until", 0.0),
                      ("_art_src", ""), ("_art", b""), ("_art_hash", "")):
        monkeypatch.setattr(lastfm, name, val)


class FakeResp:
    def __init__(self, body=None, content=b"", status=200):
        self._body, self.content, self.status_code = body, content, status

    def json(self):
        return self._body

    def raise_for_status(self):
        if self.status_code >= 400:
            raise httpx.HTTPStatusError("err", request=None, response=None)


def patch_client(monkeypatch, api_body=None, exc=None):
    calls = []

    class FakeClient:
        def __init__(self, *a, **k): pass
        async def __aenter__(self): return self
        async def __aexit__(self, *a): return False
        async def get(self, url, **k):
            calls.append(url)
            if exc: raise exc
            if url == lastfm.API_URL:
                return FakeResp(api_body)
            return FakeResp(content=png_bytes())

    monkeypatch.setattr(lastfm.httpx, "AsyncClient", FakeClient)
    return calls


def track(nowplaying=True):
    t = {"name": "Song", "artist": {"#text": "Band"}, "album": {"#text": "Album"},
         "image": [{"size": "small", "#text": "https://img/34.png"},
                   {"size": "medium", "#text": "https://img/64.png"}]}
    if nowplaying:
        t["@attr"] = {"nowplaying": "true"}
    return {"recenttracks": {"track": [t]}}


async def test_not_configured_no_http(monkeypatch):
    monkeypatch.setenv("LASTFM_USER", "")
    calls = patch_client(monkeypatch, track())
    assert await lastfm.get_now_playing() == {"is_playing": False}
    assert calls == []


async def test_nowplaying_track_and_artwork(monkeypatch):
    calls = patch_client(monkeypatch, track())
    np = await lastfm.get_now_playing()
    assert np["is_playing"] and np["title"] == "Song" and np["artist"] == "Band"
    assert np["album"] == "Album" and np["duration_ms"] == 0
    assert np["artwork_url"].startswith("https://example.com/nowplaying/artwork.jpg?v=")
    assert "https://img/64.png" in calls
    art = lastfm.get_artwork()
    assert art[:2] == b"\xff\xd8" and len(art) <= lastfm.ART_MAX_BYTES
    assert Image.open(io.BytesIO(art)).size == (64, 64)


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


def test_bad_image_gives_empty():
    assert lastfm.to_panel_jpeg(b"not an image") == b""
