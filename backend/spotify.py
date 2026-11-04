# backend/spotify.py
"""Spotify OAuth2 proxy — auto-refreshes access token and exposes now-playing info."""

import os
import time
import logging
from typing import Optional

import httpx

logger = logging.getLogger(__name__)

# --- Token state (module-level singleton) ---
_access_token: Optional[str] = None
_token_expires_at: float = 0.0

SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
SPOTIFY_NOW_PLAYING_URL = "https://api.spotify.com/v1/me/player/currently-playing"

# Spotify serves album art at 640, 300, and 64 px.
# We want the smallest (64×64) to fit ESP32 RAM comfortably.
_PREFERRED_ART_SIZE = 64

_DEFAULT_BACKOFF_S = 60.0
_FORBIDDEN_BACKOFF_S = 600.0  # 403: retry every 10 min
_backoff_until: float = 0.0


def _backoff(seconds: float = _DEFAULT_BACKOFF_S) -> None:
    global _backoff_until
    _backoff_until = time.time() + seconds


def _retry_after(resp) -> float:
    try:
        return max(1.0, float(resp.headers.get("Retry-After", _DEFAULT_BACKOFF_S)))
    except (TypeError, ValueError):
        return _DEFAULT_BACKOFF_S


def _creds_configured() -> bool:
    return all(os.getenv(k) for k in
               ("SPOTIFY_CLIENT_ID", "SPOTIFY_CLIENT_SECRET", "SPOTIFY_REFRESH_TOKEN"))


async def _refresh_access_token() -> str:
    """Exchange the long-lived refresh token for a fresh access token."""
    global _access_token, _token_expires_at

    client_id = os.getenv("SPOTIFY_CLIENT_ID", "")
    client_secret = os.getenv("SPOTIFY_CLIENT_SECRET", "")
    refresh_token = os.getenv("SPOTIFY_REFRESH_TOKEN", "")

    async with httpx.AsyncClient(timeout=10.0) as client:
        resp = await client.post(
            SPOTIFY_TOKEN_URL,
            data={
                "grant_type": "refresh_token",
                "refresh_token": refresh_token,
            },
            auth=(client_id, client_secret),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        resp.raise_for_status()
        data = resp.json()

    _access_token = data["access_token"]
    # Spotify tokens last 3600s; refresh 5 min early to be safe
    _token_expires_at = time.time() + data.get("expires_in", 3600) - 300
    logger.info("Spotify access token refreshed, expires in %ss", data.get("expires_in"))
    return _access_token


async def _get_token() -> str:
    """Return a valid access token, refreshing if needed."""
    if _access_token is None or time.time() >= _token_expires_at:
        return await _refresh_access_token()
    return _access_token


def _pick_artwork_url(images: list[dict], preferred_size: int = _PREFERRED_ART_SIZE) -> str:
    """Pick the image closest to the preferred pixel size."""
    if not images:
        return ""
    # Sort by absolute distance to preferred size
    scored = sorted(images, key=lambda img: abs((img.get("width") or 640) - preferred_size))
    return scored[0].get("url", "")


async def _get_now_playing() -> dict:
    """
    Call Spotify's currently-playing endpoint and return a simplified dict.

    Returns:
        {
            "is_playing": bool,
            "title": str,
            "artist": str,
            "album": str,
            "artwork_url": str,   # 64×64 JPEG URL from Spotify CDN
            "progress_ms": int,
            "duration_ms": int,
        }
    """
    token = await _get_token()

    async with httpx.AsyncClient(timeout=10.0) as client:
        resp = await client.get(
            SPOTIFY_NOW_PLAYING_URL,
            headers={"Authorization": f"Bearer {token}"},
        )

    # 204 = nothing playing, 200 = something playing
    if resp.status_code == 204 or resp.status_code == 202:
        return {"is_playing": False}

    if resp.status_code == 429:
        _backoff(_retry_after(resp))
        logger.warning("Spotify rate limited, backing off %.0fs", _backoff_until - time.time())
        return {"is_playing": False}

    if resp.status_code == 401:
        # Token expired mid-flight — force refresh and retry once
        token = await _refresh_access_token()
        async with httpx.AsyncClient() as client:
            resp = await client.get(
                SPOTIFY_NOW_PLAYING_URL,
                headers={"Authorization": f"Bearer {token}"},
            )

    if resp.status_code == 403:
        # Not fixable by retrying (e.g. app owner lacks Premium): poll rarely.
        _backoff(_FORBIDDEN_BACKOFF_S)
        logger.warning("Spotify API 403, backing off %.0fs: %s",
                       _FORBIDDEN_BACKOFF_S, resp.text[:200])
        return {"is_playing": False}

    if resp.status_code != 200:
        logger.warning("Spotify API returned %s: %s", resp.status_code, resp.text[:200])
        return {"is_playing": False}

    data = resp.json()

    # Could be a track or an episode (podcast)
    item = data.get("item")
    if item is None:          # ad / unknown media: nothing displayable
        return {"is_playing": False}

    item_type = item.get("type", "track")

    if item_type == "track":
        artists = ", ".join(a["name"] for a in item.get("artists", []))
        album = item.get("album", {})
        artwork_url = _pick_artwork_url(album.get("images", []))
        album_name = album.get("name", "")
    else:
        # Podcast episode
        artists = item.get("show", {}).get("name", "")
        artwork_url = _pick_artwork_url(item.get("images", []))
        album_name = ""

    return {
        "is_playing": data.get("is_playing", False),
        "title": item.get("name", ""),
        "artist": artists,
        "album": album_name,
        "artwork_url": artwork_url,
        "progress_ms": data.get("progress_ms", 0),
        "duration_ms": item.get("duration_ms", 0),
    }


async def get_now_playing() -> dict:
    """Never raises: any Spotify/network problem yields {"is_playing": False}."""
    if not _creds_configured() or time.time() < _backoff_until:
        return {"is_playing": False}
    try:
        return await _get_now_playing()
    except (httpx.HTTPError, KeyError, ValueError) as exc:
        logger.warning("Spotify now-playing failed (%s: %s); backing off %.0fs",
                       type(exc).__name__, exc, _DEFAULT_BACKOFF_S)
        _backoff()
        return {"is_playing": False}
