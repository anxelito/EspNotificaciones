"""Now playing from Last.fm (works with Spotify Free, from any device).

Spotify → Last.fm scrobbling is enabled once in the user's Last.fm settings;
Last.fm then reports the current track as "nowplaying" in
user.getRecentTracks. No progress/duration is available, so both are 0 and the
panel hides its progress bar.

artwork_url is Last.fm's own image URL; main.py passes it through
artwork.panel_url() like Spotify's.

Env: LASTFM_API_KEY, LASTFM_USER (feature off if either is empty).
"""
import logging
import os
import time

import httpx

logger = logging.getLogger(__name__)

API_URL = "https://ws.audioscrobbler.com/2.0/"
CACHE_S = 2.5            # several panels / fast polls share one Last.fm call
_BACKOFF_S = 60.0

_cache: dict | None = None
_cache_at: float = 0.0
_backoff_until: float = 0.0


def configured() -> bool:
    return bool(os.getenv("LASTFM_API_KEY") and os.getenv("LASTFM_USER"))


def _pick_image(images: list) -> str:
    """Largest Last.fm image (300 px 'extralarge'); it is downscaled later."""
    by_size = {i.get("size"): i.get("#text", "") for i in images or [] if i.get("#text")}
    for size in ("extralarge", "large", "medium", "small"):
        if by_size.get(size):
            return by_size[size]
    return ""


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
                result = {
                    "is_playing": True,
                    "title": track.get("name", ""),
                    "artist": (track.get("artist") or {}).get("#text", ""),
                    "album": (track.get("album") or {}).get("#text", ""),
                    "artwork_url": _pick_image(track.get("image")),
                    "progress_ms": 0,
                    "duration_ms": 0,
                }
    except (httpx.HTTPError, ValueError, KeyError, IndexError, AttributeError) as exc:
        _backoff_until = now + _BACKOFF_S
        logger.warning("Last.fm now-playing failed (%s: %s); backing off %.0fs",
                       type(exc).__name__, exc, _BACKOFF_S)

    _cache, _cache_at = result, now
    return result
