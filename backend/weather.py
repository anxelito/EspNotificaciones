"""Outside weather for the panel's room window (Open-Meteo, free, no API key).

Env: WEATHER_LAT, WEATHER_LON (decimal degrees). Without them the endpoint
returns {"ok": false} and the panel's window just shows the sky by the hour.
Cached for CACHE_S so many panels / polls share one upstream call.
"""
import logging
import os
import time

import httpx

logger = logging.getLogger(__name__)

API_URL = "https://api.open-meteo.com/v1/forecast"
CACHE_S = 600.0
_ERROR_RETRY_S = 120.0

_cache: dict | None = None
_cache_at: float = 0.0


def configured() -> bool:
    return bool(os.getenv("WEATHER_LAT") and os.getenv("WEATHER_LON"))


async def get_weather() -> dict:
    """{"ok": True, "temp": °C, "code": WMO code, "is_day": bool} or {"ok": False}."""
    global _cache, _cache_at
    if not configured():
        return {"ok": False}
    now = time.time()
    if _cache is not None and now - _cache_at < (CACHE_S if _cache.get("ok") else _ERROR_RETRY_S):
        return _cache

    result = {"ok": False}
    try:
        async with httpx.AsyncClient(timeout=8.0) as client:
            resp = await client.get(API_URL, params={
                "latitude": os.getenv("WEATHER_LAT"),
                "longitude": os.getenv("WEATHER_LON"),
                "current": "temperature_2m,weather_code,is_day",
                "timezone": "auto",
            })
            resp.raise_for_status()
            cur = resp.json()["current"]
            result = {
                "ok": True,
                "temp": float(cur["temperature_2m"]),
                "code": int(cur["weather_code"]),
                "is_day": bool(cur.get("is_day", 1)),
            }
    except (httpx.HTTPError, KeyError, ValueError, TypeError) as exc:
        logger.warning("weather fetch failed (%s: %s)", type(exc).__name__, exc)
        if _cache is not None and _cache.get("ok"):
            result = _cache            # keep the last good reading
    _cache, _cache_at = result, now
    return result
