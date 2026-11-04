"""PC now-playing agent: tells the panel's backend what Spotify plays on this PC.

Works with Spotify Free. Reads the Windows media session ("Global System Media
Transport Controls" -- what the volume flyout shows) for the Spotify desktop
app and pushes title/artist/album + a 64x64 JPEG cover to the backend:

    POST /nowplaying/pc            JSON state (heartbeat every POLL_S)
    PUT  /nowplaying/pc/artwork    JPEG bytes (only when the track changes)

The backend prefers the Spotify Web API when it works (Premium) and falls back
to this. If the PC is off, the pushed state goes stale on the backend and the
panel hides the music screen by itself.

Usage:
    pip install -r requirements.txt
    copy config.example.json config.json   (fill base_url + api_key)
    python nowplaying_agent.py             # run in a console (logs to stdout)
    python nowplaying_agent.py --install   # start hidden at Windows login
    python nowplaying_agent.py --uninstall
"""
import asyncio
import io
import json
import logging
import logging.handlers
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

from PIL import Image
from winrt.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as SessionManager,
    GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus,
)
from winrt.windows.storage.streams import DataReader

HERE = Path(__file__).resolve().parent
CONFIG = HERE / "config.json"
LOG_FILE = HERE / "agent.log"
STARTUP_CMD = (Path(os.environ.get("APPDATA", "")) / "Microsoft/Windows/Start Menu/Programs"
               / "Startup/panel_nowplaying_agent.cmd")

POLL_S = 3.0            # push interval while playing (backend stale after 15 s)
ART_PX = 64             # panel artwork size
ART_MAX_BYTES = 8192    # panel artwork buffer
HTTP_TIMEOUT_S = 8

log = logging.getLogger("agent")


# --- config / HTTP --------------------------------------------------------------

def load_config() -> tuple[str, str]:
    try:
        cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
        return cfg["base_url"].rstrip("/"), cfg["api_key"]
    except (OSError, ValueError, KeyError) as exc:
        sys.exit(f"Bad or missing {CONFIG} ({exc}). Copy config.example.json and fill it in.")


def http(method: str, url: str, api_key: str, body: bytes, ctype: str) -> bool:
    req = urllib.request.Request(url, data=body, method=method, headers={
        "X-API-Key": api_key, "Content-Type": ctype, "User-Agent": "panel-nowplaying-agent"})
    try:
        with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            return 200 <= resp.status < 300
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        log.warning("%s %s failed: %s", method, url, exc)
        return False


# --- media session ----------------------------------------------------------------

def is_spotify(session) -> bool:
    return "spotify" in (session.source_app_user_model_id or "").lower()


async def find_spotify(manager):
    for s in manager.get_sessions():
        if is_spotify(s):
            return s
    return None


async def read_thumbnail(props) -> bytes:
    """Cover as a small baseline JPEG (what the panel's decoder expects), or b''."""
    if props.thumbnail is None:
        return b""
    try:
        stream = await props.thumbnail.open_read_async()
        size = int(stream.size)
        if size <= 0:
            return b""
        reader = DataReader(stream)
        await reader.load_async(size)
        raw = bytearray(size)
        reader.read_bytes(raw)
        img = Image.open(io.BytesIO(bytes(raw))).convert("RGB")
        img = img.resize((ART_PX, ART_PX), Image.LANCZOS)
        for quality in (85, 70, 55, 40):
            out = io.BytesIO()
            img.save(out, "JPEG", quality=quality, progressive=False, optimize=False)
            if out.tell() <= ART_MAX_BYTES:
                return out.getvalue()
    except Exception as exc:  # thumbnail is best-effort; never kill the loop
        log.warning("thumbnail failed: %s", exc)
    return b""


# --- main loop --------------------------------------------------------------------

async def run() -> None:
    base, key = load_config()
    manager = await SessionManager.request_async()
    last_track = None       # (title, artist) whose artwork was uploaded
    was_playing = False
    log.info("agent started, pushing to %s", base)

    while True:
        try:
            session = await find_spotify(manager)
            playing = False
            if session is not None:
                info = session.get_playback_info()
                playing = info.playback_status == PlaybackStatus.PLAYING

            if playing:
                props = await session.try_get_media_properties_async()
                title, artist = props.title or "", props.artist or ""
                album = props.album_title or ""
                track = (title, artist)
                if track != last_track and title:
                    art = await read_thumbnail(props)
                    # Upload cover before the state so the panel fetches the new one.
                    http("PUT", f"{base}/nowplaying/pc/artwork", key, art, "image/jpeg")
                    last_track = track
                    log.info("now playing: %s - %s", artist, title)
                http("POST", f"{base}/nowplaying/pc", key, json.dumps({
                    "is_playing": True, "title": title, "artist": artist, "album": album,
                }).encode(), "application/json")
            elif was_playing:
                http("POST", f"{base}/nowplaying/pc", key,
                     b'{"is_playing": false}', "application/json")
                log.info("paused/stopped")
            was_playing = playing
        except Exception as exc:  # e.g. Spotify closed mid-call
            log.warning("poll error: %s", exc)
        await asyncio.sleep(POLL_S)


def install() -> None:
    pythonw = Path(sys.executable).with_name("pythonw.exe")
    if not pythonw.exists():
        pythonw = Path(sys.executable)
    STARTUP_CMD.write_text(
        f'@echo off\r\nstart "" "{pythonw}" "{Path(__file__).resolve()}"\r\n', encoding="utf-8")
    print(f"Installed: {STARTUP_CMD}\nIt will start hidden at next login. Log: {LOG_FILE}")


def main() -> None:
    if "--install" in sys.argv:
        install()
        return
    if "--uninstall" in sys.argv:
        STARTUP_CMD.unlink(missing_ok=True)
        print(f"Removed {STARTUP_CMD}")
        return
    handlers = [logging.StreamHandler()] if sys.stdout else []
    handlers.append(logging.handlers.RotatingFileHandler(
        LOG_FILE, maxBytes=200_000, backupCount=1, encoding="utf-8"))
    logging.basicConfig(level=logging.INFO, handlers=handlers,
                        format="%(asctime)s %(levelname)s %(message)s")
    asyncio.run(run())


if __name__ == "__main__":
    main()
