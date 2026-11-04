"""Now playing from Last.fm (works with Spotify Free, from any device).

Spotify → Last.fm scrobbling is enabled once in the user's Last.fm settings;
Last.fm then reports the current track as "nowplaying" in
user.getRecentTracks. No progress/duration is available, so both are 0 and the
panel hides its progress bar.

Cover art from Last.fm may be PNG or JPEG of varying size, while the panel
decodes only small baseline JPEGs, so the cover is fetched here, converted to
a 64×64 baseline JPEG and served by the backend at ARTWORK_PATH.

Env: LASTFM_API_KEY, LASTFM_USER (feature off if either is empty).
"""
import hashlib
import io
import logging
import os
import time

import httpx
from PIL import Image

logger = logging.getLogger(__name__)

API_URL = "https://ws.audioscrobbler.com/2.0/"
ARTWORK_PATH = "/nowplaying/artwork.jpg"
ART_PX = 64
ART_MAX_BYTES = 8192
CACHE_S = 2.5            # several panels / fast polls share one Last.fm call
_BACKOFF_S = 60.0

_cache: dict | None = None
_cache_at: float = 0.0
_backoff_until: float = 0.0
_art_src: str = ""       # Last.fm image URL the stored JPEG came from
_art: bytes = b""
_art_hash: str = ""


def configured() -> bool:
    return bool(os.getenv("LASTFM_API_KEY") and os.getenv("LASTFM_USER"))


def get_artwork() -> bytes:
    return _art


def to_panel_jpeg(raw: bytes) -> bytes:
    """Any image → 64×64 baseline JPEG that fits the panel buffer, or b''."""
    try:
        img = Image.open(io.BytesIO(raw)).convert("RGB").resize((ART_PX, ART_PX), Image.LANCZOS)
    except Exception:  # corrupt / unsupported image
        return b""
    for quality in (85, 70, 55, 40):
        out = io.BytesIO()
        img.save(out, "JPEG", quality=quality, progressive=False)
        if out.tell() <= ART_MAX_BYTES:
            return out.getvalue()
    return b""


def _pick_image(images: list) -> str:
    """Smallest Last.fm image that is at least 64 px ('medium'), else any."""
    by_size = {i.get("size"): i.get("#text", "") for i in images or [] if i.get("#text")}
    for size in ("medium", "large", "extralarge", "small"):
        if by_size.get(size):
            return by_size[size]
    return ""


async def _update_artwork(client: httpx.AsyncClient, url: str) -> None:
    global _art_src, _art, _art_hash
    if url == _art_src:
        return
    _art_src = url
    _art, _art_hash = b"", ""
    if not url:
        return
    try:
        resp = await client.get(url)
        resp.raise_for_status()
        _art = to_panel_jpeg(resp.content)
        _art_hash = hashlib.sha1(_art).hexdigest()[:10] if _art else ""
    except httpx.HTTPError as exc:
        logger.warning("Last.fm artwork fetch failed: %s", exc)


async def get_now_playing() -> dict:
    """Panel-format dict; {"is_playing": False} when idle, off or failing."""
    global _cache, _cache_at, _backoff_until
    if not configured():
        return {"is_playing": False}
    now = time.time()
    if _cache is not None and now - _cache_at < CACHE_S:
        return _cache
    if now < _backoff_until:
        return {"is_playing": False}

    result = {"is_playing": False}
    try:
        async with httpx.AsyncClient(timeout=8.0) as client:
            resp = await client.get(API_URL, params={
                "method": "user.getrecenttracks",
                "user": os.getenv("LASTFM_USER"),
                "api_key": os.getenv("LASTFM_API_KEY"),
                "format": "json",
                "limit": 1,
            })
            resp.raise_for_status()
            tracks = resp.json().get("recenttracks", {}).get("track", [])
            if isinstance(tracks, dict):
                tracks = [tracks]
            track = tracks[0] if tracks else None
            if track and track.get("@attr", {}).get("nowplaying") == "true":
                await _update_artwork(client, _pick_image(track.get("image")))
                base = os.getenv("BASE_URL", "").rstrip("/")
                result = {
                    "is_playing": True,
                    "title": track.get("name", ""),
                    "artist": (track.get("artist") or {}).get("#text", ""),
                    "album": (track.get("album") or {}).get("#text", ""),
                    "artwork_url": f"{base}{ARTWORK_PATH}?v={_art_hash}" if _art_hash else "",
                    "progress_ms": 0,
                    "duration_ms": 0,
                }
    except (httpx.HTTPError, ValueError, KeyError, IndexError, AttributeError) as exc:
        _backoff_until = now + _BACKOFF_S
        logger.warning("Last.fm now-playing failed (%s: %s); backing off %.0fs",
                       type(exc).__name__, exc, _BACKOFF_S)

    _cache, _cache_at = result, now
    return result
