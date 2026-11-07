# backend/database.py
import aiosqlite
from datetime import datetime
from typing import Optional
from models import Task

_DB_PATH: str = ""


def _fmt(dt: datetime) -> str:
    return dt.strftime("%H:%M %d/%m")


async def init_db(path: str) -> None:
    global _DB_PATH
    _DB_PATH = path
    async with aiosqlite.connect(_DB_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS tasks (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                text         TEXT NOT NULL,
                priority     TEXT NOT NULL DEFAULT 'normal',
                status       TEXT NOT NULL DEFAULT 'pending',
                created_at   TEXT NOT NULL,
                time_display TEXT NOT NULL
            )
        """)
        # Tasks copied from UniFlow: ext_id = the pendiente's id there,
        # ext_synced = its "done" has been written back.
        async with db.execute("PRAGMA table_info(tasks)") as cur:
            cols = {r[1] for r in await cur.fetchall()}
        if "ext_id" not in cols:
            await db.execute("ALTER TABLE tasks ADD COLUMN ext_id TEXT")
        if "ext_synced" not in cols:
            await db.execute("ALTER TABLE tasks ADD COLUMN ext_synced INTEGER NOT NULL DEFAULT 0")
        await db.execute("CREATE UNIQUE INDEX IF NOT EXISTS tasks_ext_id ON tasks (ext_id)")
        await db.commit()


def _row_to_task(row: dict) -> Task:
    return Task(
        id=row["id"],
        text=row["text"],
        priority=row["priority"],
        status=row["status"],
        created_at=row["created_at"],
        time_display=row["time_display"],
    )


async def get_all_tasks() -> list[Task]:
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("""
            SELECT * FROM tasks
            ORDER BY
                CASE
                    WHEN status='pending' AND priority='urgent' THEN 0
                    WHEN status='pending' THEN 1
                    ELSE 2
                END,
                created_at ASC
        """) as cursor:
            rows = await cursor.fetchall()
            return [_row_to_task(dict(r)) for r in rows]


async def add_task(text: str, priority: str = "normal") -> Task:
    now = datetime.now()
    created_at = now.isoformat()
    time_display = _fmt(now)
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute(
            "INSERT INTO tasks (text, priority, created_at, time_display) VALUES (?, ?, ?, ?)",
            (text, priority, created_at, time_display),
        )
        await db.commit()
        task_id = cursor.lastrowid
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT * FROM tasks WHERE id=?", (task_id,)) as cursor:
            row = await cursor.fetchone()
            return _row_to_task(dict(row))


async def set_task_done(task_id: int) -> Optional[Task]:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute(
            "UPDATE tasks SET status='done' WHERE id=?", (task_id,)
        )
        await db.commit()
        if cursor.rowcount == 0:
            return None
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT * FROM tasks WHERE id=?", (task_id,)) as cursor:
            row = await cursor.fetchone()
            return _row_to_task(dict(row)) if row else None


async def delete_task(task_id: int) -> bool:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute("DELETE FROM tasks WHERE id=?", (task_id,))
        await db.commit()
        return cursor.rowcount > 0


async def clear_done_tasks() -> int:
    async with aiosqlite.connect(_DB_PATH) as db:
        cursor = await db.execute("DELETE FROM tasks WHERE status='done'")
        await db.commit()
        return cursor.rowcount


# ---- tasks linked to an outside source (UniFlow) ----------------------------

async def get_linked_tasks() -> list[dict]:
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute(
            "SELECT id, ext_id, text, priority, status, ext_synced FROM tasks WHERE ext_id IS NOT NULL"
        ) as cursor:
            return [dict(r) for r in await cursor.fetchall()]


async def upsert_linked_task(ext_id: str, text: str, priority: str) -> None:
    """Add the outside task, or refresh its text and priority. One that is
    open again outside after its done was written back is reopened here."""
    async with aiosqlite.connect(_DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT id, status, ext_synced FROM tasks WHERE ext_id=?", (ext_id,)) as cur:
            row = await cur.fetchone()
        if row is None:
            now = datetime.now()
            await db.execute(
                "INSERT INTO tasks (text, priority, created_at, time_display, ext_id) VALUES (?, ?, ?, ?, ?)",
                (text, priority, now.isoformat(), _fmt(now), ext_id),
            )
        elif row["status"] == "done" and not row["ext_synced"]:
            pass        # done here, not written back yet: keep it done
        else:
            await db.execute(
                "UPDATE tasks SET text=?, priority=?, status='pending', ext_synced=0 WHERE id=?",
                (text, priority, row["id"]),
            )
        await db.commit()


async def mark_ext_synced(task_id: int) -> None:
    async with aiosqlite.connect(_DB_PATH) as db:
        await db.execute("UPDATE tasks SET ext_synced=1 WHERE id=?", (task_id,))
        await db.commit()
