"""Now-playing state pushed by the PC agent (pc_agent/nowplaying_agent.py).

Used when the Spotify Web API is unavailable (e.g. no Premium): the agent on
the desk PC reads Windows' media session for the Spotify app and posts it
here. State lives in memory only; it is considered stale after STALE_S, so
turning the PC off makes the music screen disappear on its own.

No progress/duration is known from this source, so both are reported as 0 and
the panel hides its progress bar.
"""
import hashlib
import os
import time

STALE_S = 15.0
MAX_ARTWORK_BYTES = 8192  # must fit the panel's artwork buffer

_state: dict | None = None
_state_at: float = 0.0
_artwork: bytes = b""
_artwork_hash: str = ""


def set_state(is_playing: bool, title: str, artist: str, album: str) -> None:
    global _state, _state_at
    _state = {
        "is_playing": bool(is_playing),
        "title": title[:120],
        "artist": artist[:120],
        "album": album[:120],
    }
    _state_at = time.time()


def set_artwork(data: bytes) -> None:
    global _artwork, _artwork_hash
    _artwork = data
    _artwork_hash = hashlib.sha1(data).hexdigest()[:10] if data else ""


def get_artwork() -> bytes:
    return _artwork


def get_fresh(now: float | None = None) -> dict | None:
    """Return the panel-format dict if the PC reported playback recently."""
    now = time.time() if now is None else now
    if _state is None or (now - _state_at) > STALE_S or not _state["is_playing"]:
        return None
    base = os.getenv("BASE_URL", "").rstrip("/")
    art = f"{base}/nowplaying/pc/artwork.jpg?v={_artwork_hash}" if _artwork_hash else ""
    return {**_state, "artwork_url": art, "progress_ms": 0, "duration_ms": 0}


def reset() -> None:
    """Test helper."""
    global _state, _state_at
    _state, _state_at = None, 0.0
    set_artwork(b"")
