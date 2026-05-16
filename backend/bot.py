# backend/bot.py
import os
from telegram import Update
from telegram.ext import (
    Application, CommandHandler, MessageHandler,
    filters, ContextTypes,
)
import database as db


def build_application() -> Application:
    app = Application.builder().token(os.getenv("TELEGRAM_BOT_TOKEN", "")).build()
    app.add_handler(CommandHandler("start", cmd_start))
    app.add_handler(CommandHandler("add", cmd_add))
    app.add_handler(CommandHandler("done", cmd_done))
    app.add_handler(CommandHandler("delete", cmd_delete))
    app.add_handler(CommandHandler("list", cmd_list))
    app.add_handler(CommandHandler("clear", cmd_clear))
    app.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, handle_text))
    return app


async def cmd_start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(
        "ESP32 Task Panel\n\n"
        "Envía texto → tarea normal\n"
        "Envía texto! → tarea urgente\n\n"
        "/add <texto> [urgent]\n"
        "/done <id>\n"
        "/delete <id>\n"
        "/list\n"
        "/clear"
    )


async def handle_text(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    text = update.message.text.strip()
    if text.endswith("!"):
        priority = "urgent"
        text = text[:-1].strip()
    else:
        priority = "normal"
    task = await db.add_task(text, priority)
    label = "URGENTE" if priority == "urgent" else "normal"
    await update.message.reply_text(f"✅ #{task.id} [{label}] {task.text}")


async def cmd_add(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /add <texto> [urgent]")
        return
    args = list(context.args)
    priority = "normal"
    if args[-1].lower() == "urgent":
        priority = "urgent"
        args = args[:-1]
    if not args:
        await update.message.reply_text("Uso: /add <texto> [urgent]")
        return
    task = await db.add_task(" ".join(args), priority)
    label = "URGENTE" if priority == "urgent" else "normal"
    await update.message.reply_text(f"✅ #{task.id} [{label}] {task.text}")


async def cmd_done(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /done <id>")
        return
    try:
        task_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("ID inválido")
        return
    task = await db.set_task_done(task_id)
    if task:
        await update.message.reply_text(f"✔️ #{task.id} completada")
    else:
        await update.message.reply_text(f"Tarea #{task_id} no encontrada")


async def cmd_delete(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    if not context.args:
        await update.message.reply_text("Uso: /delete <id>")
        return
    try:
        task_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("ID inválido")
        return
    deleted = await db.delete_task(task_id)
    if deleted:
        await update.message.reply_text(f"🗑️ #{task_id} eliminada")
    else:
        await update.message.reply_text(f"Tarea #{task_id} no encontrada")


async def cmd_list(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    tasks = await db.get_all_tasks()
    if not tasks:
        await update.message.reply_text("Sin tareas")
        return
    lines = []
    for t in tasks:
        icon = "✔️" if t.status == "done" else ("🔴" if t.priority == "urgent" else "⏳")
        lines.append(f"{icon} #{t.id} {t.text} — {t.time_display}")
    await update.message.reply_text("\n".join(lines))


async def cmd_clear(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    count = await db.clear_done_tasks()
    await update.message.reply_text(f"🗑️ {count} tarea(s) completadas eliminadas")
