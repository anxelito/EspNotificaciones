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
