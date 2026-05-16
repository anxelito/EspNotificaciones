# backend/auth.py
import hmac
import os
from fastapi import Header, HTTPException


async def verify_api_key(x_api_key: str = Header(...)) -> None:
    expected = os.getenv("API_KEY", "")
    if not hmac.compare_digest(x_api_key, expected):
        raise HTTPException(status_code=401, detail="Invalid API key")


def verify_telegram_token(token: str) -> bool:
    expected = os.getenv("TELEGRAM_SECRET_TOKEN", "")
    return hmac.compare_digest(token, expected)
