# backend/calendar_client.py
import os
import asyncio
from datetime import datetime, timedelta, date

import caldav


def _parse_event(event, today: date) -> dict | None:
    try:
        vevent = event.vobject_instance.vevent
        summary = str(vevent.summary.value)
        dtstart = vevent.dtstart.value

        if isinstance(dtstart, datetime):
            if dtstart.tzinfo is not None:
                dtstart = dtstart.astimezone().replace(tzinfo=None)
            time_display = dtstart.strftime("%H:%M")
            day = 0 if dtstart.date() == today else 1
            _sort = time_display
        else:
            # all-day: dtstart is a date object
            time_display = "Todo el dia"
            day = 0 if dtstart == today else 1
            _sort = ""

        return {
            "summary": summary[:40],
            "time_display": time_display,
            "day": day,
            "_sort": _sort,
        }
    except Exception:
        return None


def fetch_events() -> list[dict]:
    username = os.getenv("CALDAV_USERNAME", "")
    password = os.getenv("CALDAV_PASSWORD", "")
    if not username or not password:
        return []

    try:
        client = caldav.DAVClient(
            url="https://caldav.icloud.com",
            username=username,
            password=password,
        )
        principal = client.principal()
        calendars = principal.calendars()

        today = datetime.now().date()
        start = datetime.combine(today, datetime.min.time())
        end = start + timedelta(days=2)

        results = []
        for cal in calendars:
            try:
                for event in cal.date_search(start=start, end=end, expand=True):
                    parsed = _parse_event(event, today)
                    if parsed is not None:
                        results.append(parsed)
            except Exception:
                continue

        results.sort(key=lambda e: (e["day"], e["_sort"]))
        return [{"summary": e["summary"], "time_display": e["time_display"], "day": e["day"]} for e in results[:6]]
    except Exception:
        return []


async def fetch_events_async() -> list[dict]:
    return await asyncio.to_thread(fetch_events)
