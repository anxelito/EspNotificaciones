# backend/tests/test_uniflow.py
import json
import os
from datetime import date
from zoneinfo import ZoneInfo

import httpx
import pytest

import database as db
import uniflow

TODAY = date(2026, 11, 7)          # a Saturday
USER = "11111111-1111-1111-1111-111111111111"
CFG = {"url": "https://x.supabase.co", "key": "svc", "user": USER, "days": 7,
       "every": 120, "tz": ZoneInfo("Europe/Madrid")}


@pytest.fixture(autouse=True)
async def temp_db(tmp_path):
    path = str(tmp_path / "test.db")
    await db.init_db(path)
    yield
    if os.path.exists(path):
        os.unlink(path)


class FakeSupabase:
    """Answers the two PostgREST calls on the pendientes table."""

    def __init__(self, rows):
        self.rows = rows          # what the GET returns (already filtered)
        self.gets, self.patches = [], []

    def handler(self, request: httpx.Request) -> httpx.Response:
        assert request.url.path == "/rest/v1/pendientes"
        assert request.headers["apikey"] == "svc"
        assert request.headers["authorization"] == "Bearer svc"
        if request.method == "GET":
            self.gets.append(dict(request.url.params))
            return httpx.Response(200, json=self.rows)
        if request.method == "PATCH":
            self.patches.append((dict(request.url.params), json.loads(request.content)))
            return httpx.Response(204)
        return httpx.Response(405)

    def client(self):
        return httpx.AsyncClient(transport=httpx.MockTransport(self.handler))


def row(id_, titulo, due):
    return {"id": id_, "titulo": titulo, "fecha_limite": due}


def test_disabled_without_settings(monkeypatch):
    for k in ("UNIFLOW_SUPABASE_URL", "UNIFLOW_SERVICE_KEY", "UNIFLOW_USER_ID"):
        monkeypatch.delenv(k, raising=False)
    assert uniflow.config() is None
    monkeypatch.setenv("UNIFLOW_SUPABASE_URL", "https://x.supabase.co/")
    monkeypatch.setenv("UNIFLOW_SERVICE_KEY", "svc")
    assert uniflow.config() is None
    monkeypatch.setenv("UNIFLOW_USER_ID", USER)
    cfg = uniflow.config()
    assert cfg["url"] == "https://x.supabase.co" and cfg["days"] == 7


def test_labels_and_priority():
    assert uniflow.panel_task(row("a", " Práctica 2 ", "2026-11-07"), TODAY) == ("Práctica 2 · hoy", "urgent")
    assert uniflow.panel_task(row("a", "Leer", "2026-11-08"), TODAY) == ("Leer · mañana", "urgent")
    assert uniflow.panel_task(row("a", "Leer", "2026-11-05"), TODAY) == ("Leer · vencida", "urgent")
    assert uniflow.panel_task(row("a", "Examen", "2026-11-13"), TODAY) == ("Examen · vie 13", "normal")


async def test_pull_filters_on_my_account_and_window():
    fake = FakeSupabase([row("p1", "Práctica", "2026-11-07"), row("p2", "Examen", "2026-11-13")])
    async with fake.client() as c:
        await uniflow.sync_once(c, CFG, TODAY)
    q = fake.gets[0]
    assert q["user_id"] == f"eq.{USER}"
    assert q["hecho"] == "eq.false"
    assert q["fecha_limite"] == "lte.2026-11-14"
    tasks = {t.text: t for t in await db.get_all_tasks()}
    assert tasks["Práctica · hoy"].priority == "urgent"
    assert tasks["Examen · vie 13"].priority == "normal"


async def test_sync_is_idempotent_and_updates_text():
    fake = FakeSupabase([row("p1", "Práctica", "2026-11-10")])
    async with fake.client() as c:
        await uniflow.sync_once(c, CFG, TODAY)
        await uniflow.sync_once(c, CFG, TODAY)
        fake.rows = [row("p1", "Práctica final", "2026-11-08")]
        await uniflow.sync_once(c, CFG, TODAY)
    tasks = await db.get_all_tasks()
    assert [(t.text, t.priority) for t in tasks] == [("Práctica final · mañana", "urgent")]


async def test_gone_from_uniflow_is_removed_but_telegram_tasks_stay():
    await db.add_task("Comprar estaño", "normal")
    fake = FakeSupabase([row("p1", "Práctica", "2026-11-10")])
    async with fake.client() as c:
        await uniflow.sync_once(c, CFG, TODAY)
        fake.rows = []                       # done or deleted in UniFlow
        await uniflow.sync_once(c, CFG, TODAY)
    assert [t.text for t in await db.get_all_tasks()] == ["Comprar estaño"]


async def test_done_here_is_written_back_once():
    fake = FakeSupabase([row("p1", "Práctica", "2026-11-10")])
    async with fake.client() as c:
        await uniflow.sync_once(c, CFG, TODAY)
        task = (await db.get_all_tasks())[0]
        await db.set_task_done(task.id)
        fake.rows = []                       # after the PATCH it is hecho there
        await uniflow.sync_once(c, CFG, TODAY)
        await uniflow.sync_once(c, CFG, TODAY)
    assert len(fake.patches) == 1
    params, body = fake.patches[0]
    assert params == {"id": "eq.p1", "user_id": f"eq.{USER}"}
    assert body["hecho"] is True and body["hecho_at"]
    tasks = await db.get_all_tasks()
    assert [(t.text, t.status) for t in tasks] == [("Práctica · mar 10", "done")]


async def test_failed_write_back_keeps_it_done_and_retries():
    calls = {"patch": 0}

    def handler(request):
        if request.method == "PATCH":
            calls["patch"] += 1
            return httpx.Response(500 if calls["patch"] == 1 else 204)
        return httpx.Response(200, json=[row("p1", "Práctica", "2026-11-10")])

    async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as c:
        await uniflow.sync_once(c, CFG, TODAY)
        await db.set_task_done((await db.get_all_tasks())[0].id)
        with pytest.raises(httpx.HTTPStatusError):
            await uniflow.sync_once(c, CFG, TODAY)
        assert (await db.get_all_tasks())[0].status == "done"
        await uniflow.sync_once(c, CFG, TODAY)   # next pass retries the write-back
    assert calls["patch"] == 2


async def test_reopened_in_uniflow_comes_back():
    fake = FakeSupabase([row("p1", "Práctica", "2026-11-10")])
    async with fake.client() as c:
        await uniflow.sync_once(c, CFG, TODAY)
        await db.set_task_done((await db.get_all_tasks())[0].id)
        fake.rows = []
        await uniflow.sync_once(c, CFG, TODAY)        # written back
        fake.rows = [row("p1", "Práctica", "2026-11-10")]   # unticked in UniFlow
        await uniflow.sync_once(c, CFG, TODAY)
    assert [t.status for t in await db.get_all_tasks()] == ["pending"]
