import io

import httpx
import pytest
from PIL import Image

import artwork


@pytest.fixture(autouse=True)
def _reset(monkeypatch):
    monkeypatch.setenv("BASE_URL", "https://example.com")
    artwork.reset()
    yield
    artwork.reset()


def png_bytes(size=300, colour=(200, 30, 90)):
    out = io.BytesIO()
    Image.new("RGB", (size, size), colour).save(out, "PNG")
    return out.getvalue()


def patch_client(monkeypatch, content=b"", exc=None):
    calls = []

    class Resp:
        def __init__(self): self.content = content
        def raise_for_status(self): pass

    class FakeClient:
        def __init__(self, *a, **k): pass
        async def __aenter__(self): return self
        async def __aexit__(self, *a): return False
        async def get(self, url, **k):
            calls.append(url)
            if exc: raise exc
            return Resp()

    monkeypatch.setattr(artwork.httpx, "AsyncClient", FakeClient)
    return calls


async def test_converts_and_caches(monkeypatch):
    calls = patch_client(monkeypatch, png_bytes())
    url = await artwork.panel_url("https://img/a.png")
    assert url.startswith("https://example.com/nowplaying/artwork.jpg?v=")
    art = artwork.get_artwork()
    assert art[:2] == b"\xff\xd8" and len(art) <= artwork.ART_MAX_BYTES
    assert Image.open(io.BytesIO(art)).size == (artwork.ART_PX, artwork.ART_PX)
    assert await artwork.panel_url("https://img/a.png") == url
    assert len(calls) == 1          # same cover: no re-download


async def test_empty_source():
    assert await artwork.panel_url("") == ""


async def test_fetch_error_retries_later(monkeypatch):
    calls = patch_client(monkeypatch, exc=httpx.ConnectError("boom"))
    assert await artwork.panel_url("https://img/a.png") == ""
    assert await artwork.panel_url("https://img/a.png") == ""
    assert len(calls) == 2


def test_bad_image_gives_empty():
    assert artwork.to_panel_jpeg(b"not an image") == b""


def test_accent_prefers_colour_over_black():
    img = Image.new("RGB", (100, 100), (5, 5, 5))           # mostly black
    for x in range(30, 70):
        for y in range(30, 70):
            img.putpixel((x, y), (30, 120, 230))            # blue square
    out = io.BytesIO(); img.save(out, "PNG")
    r, g, b = (int(artwork.accent_colour(out.getvalue())[i:i + 2], 16) for i in (1, 3, 5))
    assert b > 150 and r < 90


async def test_panel_url_sets_accent(monkeypatch):
    patch_client(monkeypatch, png_bytes(colour=(200, 30, 90)))
    await artwork.panel_url("https://img/a.png")
    acc = artwork.get_accent()
    assert acc.startswith("#") and len(acc) == 7 and int(acc[1:3], 16) > 150
