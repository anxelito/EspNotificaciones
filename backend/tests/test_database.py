# backend/tests/test_database.py
import pytest
import os
import database as db
from models import Task


@pytest.fixture(autouse=True)
async def temp_db(tmp_path):
    path = str(tmp_path / "test.db")
    await db.init_db(path)
    yield
    if os.path.exists(path):
        os.unlink(path)


@pytest.mark.asyncio
async def test_add_normal_task():
    task = await db.add_task("Buy components", "normal")
    assert task.text == "Buy components"
    assert task.priority == "normal"
    assert task.status == "pending"
    assert isinstance(task.id, int)


@pytest.mark.asyncio
async def test_add_urgent_task():
    task = await db.add_task("Fix now", "urgent")
    assert task.priority == "urgent"


@pytest.mark.asyncio
async def test_get_all_returns_tasks():
    await db.add_task("A", "normal")
    await db.add_task("B", "urgent")
    tasks = await db.get_all_tasks()
    assert len(tasks) == 2


@pytest.mark.asyncio
async def test_sort_urgent_first():
    await db.add_task("normal task", "normal")
    await db.add_task("urgent task", "urgent")
    tasks = await db.get_all_tasks()
    assert tasks[0].priority == "urgent"


@pytest.mark.asyncio
async def test_sort_done_last():
    t_normal = await db.add_task("normal", "normal")
    t_urgent = await db.add_task("urgent", "urgent")
    await db.set_task_done(t_urgent.id)
    tasks = await db.get_all_tasks()
    assert tasks[-1].status == "done"


@pytest.mark.asyncio
async def test_set_task_done():
    task = await db.add_task("Do it", "normal")
    done = await db.set_task_done(task.id)
    assert done is not None
    assert done.status == "done"


@pytest.mark.asyncio
async def test_set_done_not_found():
    result = await db.set_task_done(9999)
    assert result is None


@pytest.mark.asyncio
async def test_delete_task():
    task = await db.add_task("Delete me", "normal")
    result = await db.delete_task(task.id)
    assert result is True
    tasks = await db.get_all_tasks()
    assert all(t.id != task.id for t in tasks)


@pytest.mark.asyncio
async def test_delete_not_found():
    result = await db.delete_task(9999)
    assert result is False


@pytest.mark.asyncio
async def test_clear_done_removes_only_done():
    t1 = await db.add_task("done", "normal")
    t2 = await db.add_task("pending", "normal")
    await db.set_task_done(t1.id)
    count = await db.clear_done_tasks()
    assert count == 1
    remaining = await db.get_all_tasks()
    assert len(remaining) == 1
    assert remaining[0].id == t2.id


@pytest.mark.asyncio
async def test_time_display_formatted():
    task = await db.add_task("Test", "normal")
    import re
    assert re.match(r"\d{2}:\d{2} \d{2}/\d{2}", task.time_display)
