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


def _fetch_from_source(url: str, username: str, password: str, today: date) -> list[dict]:
    start = datetime.combine(today, datetime.min.time())
    end = start + timedelta(days=2)
    results = []
    try:
        client = caldav.DAVClient(url=url, username=username, password=password)
        calendars = client.principal().calendars()
        for cal in calendars:
            try:
                for event in cal.date_search(start=start, end=end, expand=True):
                    parsed = _parse_event(event, today)
                    if parsed is not None:
                        results.append(parsed)
            except Exception:
                continue
    except Exception:
        pass
    return results


def fetch_events() -> list[dict]:
    today = datetime.now().date()
    results = []

    # iCloud
    icloud_user = os.getenv("CALDAV_USERNAME", "")
    icloud_pass = os.getenv("CALDAV_PASSWORD", "")
    if icloud_user and icloud_pass:
        results.extend(_fetch_from_source("https://caldav.icloud.com", icloud_user, icloud_pass, today))

    # Google Calendar
    google_user = os.getenv("GOOGLE_CALDAV_USERNAME", "")
    google_pass = os.getenv("GOOGLE_CALDAV_PASSWORD", "")
    if google_user and google_pass:
        url = f"https://www.google.com/calendar/dav/{google_user}/user"
        results.extend(_fetch_from_source(url, google_user, google_pass, today))

    # Deduplicate by (summary, day, time_display)
    seen = set()
    unique = []
    for e in results:
        key = (e["summary"], e["day"], e["time_display"])
        if key not in seen:
            seen.add(key)
            unique.append(e)

    unique.sort(key=lambda e: (e["day"], e["_sort"]))
    return [{"summary": e["summary"], "time_display": e["time_display"], "day": e["day"]} for e in unique[:6]]


async def fetch_events_async() -> list[dict]:
    return await asyncio.to_thread(fetch_events)
