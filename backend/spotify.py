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


async def _refresh_access_token() -> str:
    """Exchange the long-lived refresh token for a fresh access token."""
    global _access_token, _token_expires_at

    client_id = os.getenv("SPOTIFY_CLIENT_ID", "")
    client_secret = os.getenv("SPOTIFY_CLIENT_SECRET", "")
    refresh_token = os.getenv("SPOTIFY_REFRESH_TOKEN", "")

    async with httpx.AsyncClient() as client:
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
    scored = sorted(images, key=lambda img: abs(img.get("width", 640) - preferred_size))
    return scored[0].get("url", "")


async def get_now_playing() -> dict:
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

    async with httpx.AsyncClient() as client:
        resp = await client.get(
            SPOTIFY_NOW_PLAYING_URL,
            headers={"Authorization": f"Bearer {token}"},
        )

    # 204 = nothing playing, 200 = something playing
    if resp.status_code == 204 or resp.status_code == 202:
        return {"is_playing": False}

    if resp.status_code == 401:
        # Token expired mid-flight — force refresh and retry once
        token = await _refresh_access_token()
        async with httpx.AsyncClient() as client:
            resp = await client.get(
                SPOTIFY_NOW_PLAYING_URL,
                headers={"Authorization": f"Bearer {token}"},
            )

    if resp.status_code != 200:
        logger.warning("Spotify API returned %s: %s", resp.status_code, resp.text[:200])
        return {"is_playing": False}

    data = resp.json()

    # Could be a track or an episode (podcast)
    item = data.get("item")
    if item is None:
        return {"is_playing": data.get("is_playing", False)}

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
