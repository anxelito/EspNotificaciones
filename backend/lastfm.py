"""Now playing from Last.fm (works with Spotify Free, from any device).

Spotify → Last.fm scrobbling is enabled once in the user's Last.fm settings;
Last.fm then reports the current track as "nowplaying" in
user.getRecentTracks. Last.fm has no playback position, so the backend
estimates it: duration from track.getInfo (or the iTunes Search API), progress
= time since this track was first seen playing (clamped). Not exact (it lags
by the scrobble delay and keeps running while paused), good enough for a bar.
Tracks without a Last.fm cover (its grey star placeholder) borrow the iTunes
artwork.

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

ITUNES_URL = "https://itunes.apple.com/search"
_PLACEHOLDER = "2a96cbd8b46e442fc41c2b86b821562f"   # Last.fm's "no cover" star
_EXTRA_MAX = 200                                     # cached tracks

_cache: dict | None = None
_extra: dict = {}          # (artist, title) -> (duration_ms, art_url)
_np_key: tuple | None = None
_np_since: float = 0.0
_cache_at: float = 0.0
_backoff_until: float = 0.0


def configured() -> bool:
    return bool(os.getenv("LASTFM_API_KEY") and os.getenv("LASTFM_USER"))


def _pick_image(images: list) -> str:
    """Largest Last.fm image (300 px 'extralarge'); it is downscaled later."""
    by_size = {i.get("size"): i.get("#text", "") for i in images or [] if i.get("#text")}
    for size in ("extralarge", "large", "medium", "small"):
        url = by_size.get(size)
        if url:
            return "" if _PLACEHOLDER in url else url
    return ""


async def _track_extra(client: httpx.AsyncClient, artist: str, title: str, have_art: bool):
    """(duration_ms, art_url) for a track, cached; 0 / '' when unknown."""
    key = (artist.lower(), title.lower())
    if key in _extra:
        return _extra[key]
    duration, art = 0, ""
    try:
        resp = await client.get(API_URL, params={
            "method": "track.getInfo", "artist": artist, "track": title,
            "api_key": os.getenv("LASTFM_API_KEY"), "format": "json", "autocorrect": 1,
        })
        resp.raise_for_status()
        duration = int((resp.json().get("track") or {}).get("duration") or 0)
    except (httpx.HTTPError, ValueError, TypeError, AttributeError) as exc:
        logger.info("Last.fm track.getInfo failed: %s", exc)
    if not duration or not have_art:
        try:
            resp = await client.get(ITUNES_URL, params={
                "term": f"{artist} {title}", "entity": "song", "limit": 1})
            resp.raise_for_status()
            hits = resp.json().get("results") or []
            if hits:
                duration = duration or int(hits[0].get("trackTimeMillis") or 0)
                a100 = hits[0].get("artworkUrl100") or ""
                art = a100.replace("100x100bb", "300x300bb") if a100 else ""
        except (httpx.HTTPError, ValueError, TypeError, AttributeError) as exc:
            logger.info("iTunes search failed: %s", exc)
    if len(_extra) >= _EXTRA_MAX:
        _extra.clear()
    _extra[key] = (duration, art)
    return _extra[key]


def _progress_ms(key: tuple, duration: int, now: float) -> int:
    """Estimated position: time since this track was first seen playing."""
    global _np_key, _np_since
    if key != _np_key:
        _np_key, _np_since = key, now
    if not duration:
        return 0
    return min(int((now - _np_since) * 1000), duration)


def _forget_track() -> None:
    global _np_key
    _np_key = None


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
                title = track.get("name", "")
                artist = (track.get("artist") or {}).get("#text", "")
                art = _pick_image(track.get("image"))
                duration, extra_art = await _track_extra(client, artist, title, bool(art))
                progress = _progress_ms((artist, title), duration, now)
                result = {
                    "is_playing": True,
                    "title": title,
                    "artist": artist,
                    "album": (track.get("album") or {}).get("#text", ""),
                    "artwork_url": art or extra_art,
                    "progress_ms": progress,
                    "duration_ms": duration,
                }
            else:
                _forget_track()
    except (httpx.HTTPError, ValueError, KeyError, IndexError, AttributeError) as exc:
        _backoff_until = now + _BACKOFF_S
        logger.warning("Last.fm now-playing failed (%s: %s); backing off %.0fs",
                       type(exc).__name__, exc, _BACKOFF_S)

    _cache, _cache_at = result, now
    return result
