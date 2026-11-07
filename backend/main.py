# backend/main.py
import asyncio
import os
import unicodedata
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request, HTTPException, Response
from dotenv import load_dotenv
from telegram import Update

load_dotenv()

_REQUIRED_ENV = ["TELEGRAM_BOT_TOKEN", "TELEGRAM_SECRET_TOKEN", "API_KEY", "BASE_URL"]
for _var in _REQUIRED_ENV:
    if not os.getenv(_var):
        raise RuntimeError(f"Missing required env var: {_var}")

from database import init_db, get_all_tasks
from auth import verify_api_key, verify_telegram_token
from bot import build_application
from spotify import get_now_playing
import lastfm
import artwork
import weather
import uniflow

_bot_app = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global _bot_app
    await init_db(os.getenv("DATABASE_URL", "./tasks.db"))
    _bot_app = build_application()
    await _bot_app.initialize()
    await _bot_app.bot.set_webhook(
        url=f"{os.getenv('BASE_URL')}/webhook",
        secret_token=os.getenv("TELEGRAM_SECRET_TOKEN"),
    )
    app.state.bot_app = _bot_app
    cfg = uniflow.config()
    sync_task = asyncio.create_task(uniflow.run_forever(cfg)) if cfg else None
    yield
    if sync_task:
        sync_task.cancel()
    await _bot_app.shutdown()
    _bot_app = None


app = FastAPI(lifespan=lifespan)


@app.get("/health")
async def health():
    return {"status": "ok"}


@app.get("/tasks")
async def tasks_endpoint(_=Depends(verify_api_key)):
    tasks = await get_all_tasks()
    out = []
    for t in tasks:
        d = t.to_dict()
        d["text"] = _panel_text(d.get("text") or "")
        out.append(d)
    return out


@app.get("/nowplaying")
async def nowplaying_endpoint(_=Depends(verify_api_key)):
    # Spotify Web API first (any device, has progress; needs Premium on the
    # app owner's account). Fall back to Last.fm scrobbles (no progress bar).
    np = await get_now_playing()
    if not np.get("is_playing"):
        lf = await lastfm.get_now_playing()
        if not lf.get("is_playing"):
            return np
        np = lf
    np = _panel_np_text(np)
    np["artwork_url"] = await artwork.panel_url(np.get("artwork_url") or "")
    if np["artwork_url"]:
        if artwork.get_accent():
            np["accent"] = artwork.get_accent()
        if artwork.get_bg():
            np["bg"] = artwork.get_bg()
    return np


@app.get("/weather")
async def weather_endpoint(_=Depends(verify_api_key)):
    return await weather.get_weather()


def _panel_text(text: str) -> str:
    """Keep what the panel's fonts can draw (ASCII + Latin-1: accents, ñ, ¿,
    ¡, °, ·); fold anything else ('–' -> '-', 'ł' -> 'l', emoji dropped)."""
    text = (text.replace("–", "-").replace("—", "-")
                .replace("‘", "'").replace("’", "'")
                .replace("“", '"').replace("”", '"')
                .replace("…", "...").replace(" ", " "))
    out = []
    for ch in text:
        if ord(ch) <= 0xFF:
            out.append(ch)
        else:
            out.append(unicodedata.normalize("NFKD", ch).encode("ascii", "ignore").decode("ascii"))
    return "".join(out)


def _panel_np_text(np: dict) -> dict:
    return {k: _panel_text(v) if k in ("title", "artist", "album") and isinstance(v, str) else v
            for k, v in np.items()}


@app.get(artwork.ARTWORK_PATH)
async def nowplaying_artwork():
    # Public on purpose: the panel's artwork fetch sends no API key, and the
    # image is just the current album cover.
    data = artwork.get_artwork()
    if not data:
        raise HTTPException(status_code=404, detail="No artwork")
    return Response(content=data, media_type="image/jpeg")


@app.post("/webhook")
async def webhook(request: Request):
    token = request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")
    if not verify_telegram_token(token):
        raise HTTPException(status_code=403, detail="Forbidden")
    data = await request.json()
    update = Update.de_json(data, app.state.bot_app.bot)
    await app.state.bot_app.process_update(update)
    return {"ok": True}
