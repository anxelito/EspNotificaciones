# backend/main.py
import os
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request, HTTPException
from dotenv import load_dotenv
from telegram import Update

load_dotenv()

from database import init_db, get_all_tasks
from auth import verify_api_key, verify_telegram_token
from bot import build_application

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


@app.post("/webhook")
async def webhook(request: Request):
    global _bot_app
    token = request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")
    if not verify_telegram_token(token):
        raise HTTPException(status_code=403, detail="Forbidden")
    if _bot_app is None:
        _bot_app = build_application()
    data = await request.json()
    update = Update.de_json(data, _bot_app.bot)
    await _bot_app.process_update(update)
    return {"ok": True}
