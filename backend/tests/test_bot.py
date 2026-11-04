# backend/tests/test_bot.py
import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from telegram import Update, Message, Chat, User
from telegram.ext import ContextTypes
from models import Task
from datetime import datetime
import database as db
import bot


def _mock_task(id=1, text="Test", priority="normal", status="pending"):
    return Task(id, text, priority, status, datetime.now().isoformat(), "14:30 16/05")


def make_update(text: str, args: list[str] | None = None):
    message = MagicMock(spec=Message)
    message.text = text
    message.reply_text = AsyncMock()
    update = MagicMock(spec=Update)
    update.message = message
    context = MagicMock(spec=ContextTypes.DEFAULT_TYPE)
    context.args = args if args is not None else []
    return update, context


@pytest.mark.asyncio
async def test_plain_text_creates_normal_task():
    update, context = make_update("Comprar pines")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Comprar pines")) as mock_add:
        await bot.handle_text(update, context)
    mock_add.assert_called_once_with("Comprar pines", "normal")
    update.message.reply_text.assert_called_once()


@pytest.mark.asyncio
async def test_bang_creates_urgent_task():
    update, context = make_update("Arreglar cortocircuito!")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Arreglar cortocircuito", priority="urgent")) as mock_add:
        await bot.handle_text(update, context)
    mock_add.assert_called_once_with("Arreglar cortocircuito", "urgent")


@pytest.mark.asyncio
async def test_bang_stripped_from_text():
    update, context = make_update("Fix now!")
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Fix now", priority="urgent")) as mock_add:
        await bot.handle_text(update, context)
    call_text = mock_add.call_args[0][0]
    assert not call_text.endswith("!")


@pytest.mark.asyncio
async def test_cmd_add_normal():
    update, context = make_update("/add Soldar pines", args=["Soldar", "pines"])
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Soldar pines")) as mock_add:
        await bot.cmd_add(update, context)
    mock_add.assert_called_once_with("Soldar pines", "normal")


@pytest.mark.asyncio
async def test_cmd_add_urgent_flag():
    update, context = make_update("/add Fix this urgent", args=["Fix", "this", "urgent"])
    with patch.object(db, "add_task", new_callable=AsyncMock,
                      return_value=_mock_task(text="Fix this", priority="urgent")) as mock_add:
        await bot.cmd_add(update, context)
    mock_add.assert_called_once_with("Fix this", "urgent")


@pytest.mark.asyncio
async def test_cmd_add_no_args():
    update, context = make_update("/add", args=[])
    await bot.cmd_add(update, context)
    update.message.reply_text.assert_called_once()
    assert "Uso:" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_done_marks_task():
    update, context = make_update("/done 1", args=["1"])
    with patch.object(db, "set_task_done", new_callable=AsyncMock,
                      return_value=_mock_task(status="done")) as mock_done:
        await bot.cmd_done(update, context)
    mock_done.assert_called_once_with(1)
    update.message.reply_text.assert_called_once()


@pytest.mark.asyncio
async def test_cmd_done_not_found():
    update, context = make_update("/done 99", args=["99"])
    with patch.object(db, "set_task_done", new_callable=AsyncMock, return_value=None):
        await bot.cmd_done(update, context)
    assert "no encontrada" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_done_invalid_id():
    update, context = make_update("/done abc", args=["abc"])
    await bot.cmd_done(update, context)
    assert "inválido" in update.message.reply_text.call_args[0][0].lower()


@pytest.mark.asyncio
async def test_cmd_delete_found():
    update, context = make_update("/delete 1", args=["1"])
    with patch.object(db, "delete_task", new_callable=AsyncMock, return_value=True):
        await bot.cmd_delete(update, context)
    assert "#1" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_list_empty():
    update, context = make_update("/list")
    with patch.object(db, "get_all_tasks", new_callable=AsyncMock, return_value=[]):
        await bot.cmd_list(update, context)
    assert "Sin tareas" in update.message.reply_text.call_args[0][0]


@pytest.mark.asyncio
async def test_cmd_clear():
    update, context = make_update("/clear")
    with patch.object(db, "clear_done_tasks", new_callable=AsyncMock, return_value=2):
        await bot.cmd_clear(update, context)
    assert "2" in update.message.reply_text.call_args[0][0]


# ---- TELEGRAM_ALLOWED_USERS ---------------------------------------------------

def test_allowed_filter_open_when_unset(monkeypatch):
    monkeypatch.delenv("TELEGRAM_ALLOWED_USERS", raising=False)
    assert bot.allowed_users() is None


def test_allowed_filter_parses_ids(monkeypatch):
    monkeypatch.setenv("TELEGRAM_ALLOWED_USERS", " 123, 456 ,")
    assert bot.allowed_users() == {123, 456}


def test_handlers_only_answer_allowed_users(monkeypatch):
    monkeypatch.setenv("TELEGRAM_BOT_TOKEN", "123:ABC")
    monkeypatch.setenv("TELEGRAM_ALLOWED_USERS", "42")
    app = bot.build_application()
    handlers = app.handlers[0]
    assert handlers, "no handlers registered"

    def upd(uid, text):
        u = MagicMock(spec=Update)
        u.effective_user = User(id=uid, first_name="x", is_bot=False)
        msg = MagicMock(spec=Message)
        msg.text = text
        msg.from_user = u.effective_user
        u.effective_message = msg
        u.message = msg
        return u

    for h in handlers:
        text = "/list" if h.__class__.__name__ == "CommandHandler" else "hola"
        if h.__class__.__name__ == "CommandHandler":
            text = "/" + sorted(h.commands)[0]
        assert not h.filters.check_update(upd(7, text)), f"{h} lets user 7 through"
        assert h.filters.check_update(upd(42, text)), f"{h} blocks allowed user 42"
