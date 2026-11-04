# backend/main.py
import os
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request, HTTPException, Response
from pydantic import BaseModel
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
import pc_nowplaying

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
    yield
    await _bot_app.shutdown()
    _bot_app = None


app = FastAPI(lifespan=lifespan)


@app.get("/health")
async def health():
    return {"status": "ok"}


@app.get("/tasks")
async def tasks_endpoint(_=Depends(verify_api_key)):
    tasks = await get_all_tasks()
    return [t.to_dict() for t in tasks]


@app.get("/nowplaying")
async def nowplaying_endpoint(_=Depends(verify_api_key)):
    # Spotify Web API first (works from any device, has progress; needs
    # Premium). Fall back to what the PC agent reports (no progress bar).
    sp = await get_now_playing()
    if sp.get("is_playing"):
        return sp
    return pc_nowplaying.get_fresh() or sp


class PcNowPlaying(BaseModel):
    is_playing: bool
    title: str = ""
    artist: str = ""
    album: str = ""


@app.post("/nowplaying/pc")
async def nowplaying_pc_push(body: PcNowPlaying, _=Depends(verify_api_key)):
    pc_nowplaying.set_state(body.is_playing, body.title, body.artist, body.album)
    return {"ok": True}


@app.put("/nowplaying/pc/artwork")
async def nowplaying_pc_artwork_put(request: Request, _=Depends(verify_api_key)):
    data = await request.body()
    if len(data) > pc_nowplaying.MAX_ARTWORK_BYTES:
        raise HTTPException(status_code=413, detail="Artwork too large")
    pc_nowplaying.set_artwork(data)
    return {"ok": True}


@app.get("/nowplaying/pc/artwork.jpg")
async def nowplaying_pc_artwork_get():
    # Public on purpose: the panel's artwork fetch sends no API key, and the
    # image is just the current album cover.
    data = pc_nowplaying.get_artwork()
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
