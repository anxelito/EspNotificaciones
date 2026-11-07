# backend/uniflow.py
"""Copies your UniFlow "pendientes" (Supabase) into the panel's tasks.

Only rows of UNIFLOW_USER_ID that are not done and are due from today up to
UNIFLOW_DAYS_AHEAD days ahead (overdue ones too) are shown; due today,
tomorrow or overdue = urgent. Marking one done here (Telegram /done) marks
it done in UniFlow; done, deleted or moved out of the window there = gone
from the panel. Disabled unless the three UNIFLOW_* settings are set.
"""
import asyncio
import logging
import os
from datetime import date, datetime, timedelta, timezone
from zoneinfo import ZoneInfo

import httpx

import database as db

log = logging.getLogger("uniflow")

_DAYS = ["lun", "mar", "mié", "jue", "vie", "sáb", "dom"]


def config() -> dict | None:
    url = os.getenv("UNIFLOW_SUPABASE_URL", "").strip().rstrip("/")
    key = os.getenv("UNIFLOW_SERVICE_KEY", "").strip()
    user = os.getenv("UNIFLOW_USER_ID", "").strip()
    if not (url and key and user):
        return None
    return {
        "url": url,
        "key": key,
        "user": user,
        "days": int(os.getenv("UNIFLOW_DAYS_AHEAD", "7")),
        "every": max(30, int(os.getenv("UNIFLOW_SYNC_SECONDS", "120"))),
        "tz": ZoneInfo(os.getenv("UNIFLOW_TZ", "Europe/Madrid")),
    }


def due_label(due: date, today: date) -> str:
    d = (due - today).days
    if d < 0:
        return "vencida"
    if d == 0:
        return "hoy"
    if d == 1:
        return "mañana"
    return f"{_DAYS[due.weekday()]} {due.day}"


def panel_task(row: dict, today: date) -> tuple[str, str]:
    """(text, priority) for one pendiente row."""
    due = date.fromisoformat(row["fecha_limite"][:10])
    text = f"{row['titulo'].strip()} · {due_label(due, today)}"
    return text, ("urgent" if (due - today).days <= 1 else "normal")


def _headers(cfg: dict) -> dict:
    return {"apikey": cfg["key"], "Authorization": f"Bearer {cfg['key']}"}


async def sync_once(client: httpx.AsyncClient, cfg: dict, today: date) -> None:
    base = f"{cfg['url']}/rest/v1/pendientes"

    # 1. Push: done here -> done in UniFlow.
    for t in await db.get_linked_tasks():
        if t["status"] == "done" and not t["ext_synced"]:
            r = await client.patch(
                base,
                params={"id": f"eq.{t['ext_id']}", "user_id": f"eq.{cfg['user']}"},
                json={"hecho": True, "hecho_at": datetime.now(timezone.utc).isoformat()},
                headers={**_headers(cfg), "Prefer": "return=minimal"},
            )
            r.raise_for_status()
            await db.mark_ext_synced(t["id"])

    # 2. Pull the open ones in the window (overdue included).
    until = today + timedelta(days=cfg["days"])
    r = await client.get(
        base,
        params={
            "select": "id,titulo,fecha_limite",
            "user_id": f"eq.{cfg['user']}",
            "hecho": "eq.false",
            "fecha_limite": f"lte.{until.isoformat()}",
            "order": "fecha_limite.asc",
        },
        headers=_headers(cfg),
    )
    r.raise_for_status()
    seen = set()
    for row in r.json():
        text, priority = panel_task(row, today)
        await db.upsert_linked_task(str(row["id"]), text, priority)
        seen.add(str(row["id"]))

    # 3. Pending here but no longer open there: remove.
    for t in await db.get_linked_tasks():
        if t["status"] == "pending" and t["ext_id"] not in seen:
            await db.delete_task(t["id"])


async def run_forever(cfg: dict) -> None:
    async with httpx.AsyncClient(timeout=10.0) as client:
        while True:
            try:
                await sync_once(client, cfg, datetime.now(cfg["tz"]).date())
            except (httpx.HTTPError, KeyError, ValueError, TypeError) as exc:
                log.warning("UniFlow sync failed: %s", exc)
            await asyncio.sleep(cfg["every"])
