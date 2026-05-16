# backend/tests/test_auth.py
import pytest
import os
from unittest.mock import MagicMock
from fastapi import HTTPException

os.environ.setdefault("API_KEY", "correct-key")
os.environ.setdefault("TELEGRAM_SECRET_TOKEN", "tg-secret")

_VALID_KEY = os.environ["API_KEY"]

from auth import verify_api_key, verify_telegram_token


@pytest.mark.asyncio
async def test_valid_api_key_passes():
    # Should not raise — use whatever key is actually configured
    await verify_api_key(x_api_key=_VALID_KEY)


@pytest.mark.asyncio
async def test_wrong_api_key_raises_401():
    with pytest.raises(HTTPException) as exc_info:
        await verify_api_key(x_api_key="wrong-key")
    assert exc_info.value.status_code == 401


def test_valid_telegram_token():
    assert verify_telegram_token("tg-secret") is True


def test_invalid_telegram_token():
    assert verify_telegram_token("wrong") is False
