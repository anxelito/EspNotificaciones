# Room Sensors, Touch Navigation & Cat Mascot — Design Spec
**Date:** 2026-11-04
**Status:** Approved
**Builds on:** `2026-05-16-esp32-task-panel-design.md` (tasks + Telegram), plus the uncommitted Spotify "Now Playing" work (`backend/spotify.py`, `firmware/src/nowplaying.h`).

## Intent

The panel today shows only remote data (Telegram tasks, Spotify). The owner wants it to feel like a living object in the room:
it should show the room's temperature and humidity, let them flip screens by touching it, look nicer, and have a small
creature (a cat) that reacts to the room — colour follows temperature, it sleeps at night.

Success = glance at the desk, see the cat, instantly know whether the room is cold/fine/hot, tap to see tasks or music.

## Goals
- Read temperature + humidity from a **DHT11** and show them.
- Navigate screens with a **TTP223** capacitive touch module (red breakout, digital output).
- New default screen "Habitación" with an animated cat mascot.
- Restyle all screens in one consistent visual language ("estilo B — tarjetas suaves").

## Non-goals
- No sending sensor data to the backend / Telegram / history graphs (not requested).
- No backend changes required for this work.
- No bitmap/sprite assets for the cat — drawn with primitives.

## Anti-goals (would count as failure even if it "works")
- The cat covering or crowding the room data at the bottom of the screen.
- Visible flicker / full-screen flashes every frame while the cat animates.
- Touch feeling laggy or missing taps because the loop is blocked by HTTP calls.

## Hardware

| Part | Notes | Pin (builder's call, see below) |
|---|---|---|
| ESP32 DevKit (`esp32dev`) | existing | — |
| ST7789 320×240, landscape, SPI | existing; uses GPIO 18, 19, 23, 15, 2, 4 | — |
| DHT11 | 1-wire data, 3.3V | GPIO 27 |
| TTP223 touch (red module) | digital out, HIGH when touched, 3.3V | GPIO 32 |

Unknowns (owner hasn't checked the hardware yet) — design must handle both cases:
- **DHT11 variant**: 3-pin breakout (has pull-up) or bare 4-pin (needs external 10 kΩ pull-up DATA→3.3V). Wiring notes must cover both.
- **Backlight**: `TFT_BL` is not defined in `User_Setup.h`, so BL is probably tied to 3.3V. Make night dimming optional: `#define BACKLIGHT_PIN -1` in `config.h`; if set to a GPIO, PWM-dim the backlight while the cat sleeps; if -1, no dimming.
- **Spotify work is uncommitted and untested on hardware.** Plan step 1: flash current firmware, verify Now Playing works, fix if needed, commit — before starting this feature.

Pin choices are **my call** (free, non-strapping, not used by the TFT). Put them in `config.h`.

## Screens & navigation (decided with owner)

Order: **Habitación → Tareas → Spotify → Habitación**.
- **Habitación** is the default/home screen.
- **Spotify** is in the cycle only while something is playing; otherwise skipped.
- **Short tap**: next screen.
- **Long press (> 1 s)**: "pet" the cat — happy face + small hop. Works on the Habitación screen (builder: on other screens, long press may simply be ignored).
- **Inactivity**: after 30 s with no touch, return to Habitación.
- **If cat is asleep**: first tap only wakes it (for a few seconds) — does not change screen. **My call:** this only applies while the Habitación screen is showing; on other screens a tap always changes screen.
- **Before NTP has ever synced (my call)**: time unknown → cat stays awake.
- **Behaviour change**: Spotify no longer auto-takes over the screen when a song starts. Instead a small **♫** indicator appears in the header on every screen.

Bottom of every screen: page-indicator dots (3, or 2 when Spotify is skipped), current one highlighted.

## Visual style — "B, tarjetas suaves" (chosen by owner)

- Background blue-black (~`#0b0d12`), not pure black. Cards `#161a24` with rounded corners (~8 px radius), 8 px side margins.
- Header (~26 px): screen name left (muted grey), time centre (light), right side: `22° · 48%` (room temp/humidity) always visible. ♫ next to time when music is playing.
- **Offline (my call)**: keep today's behaviour — time is replaced by `OFFLINE` in red when the backend is unreachable, on every screen.
- **Pending count (my call)**: shown in the Tareas screen title, e.g. `TAREAS · 2`, instead of a separate header field.
- **Tareas**: one rounded card per task; coloured dot on the left for priority (red urgent, green normal), small "urgente" chip on urgent ones, second line with time in muted text. Done tasks: struck through / dimmed (~45% look). Keep existing sort and "+N más" overflow.
- **Spotify**: rounded-corner artwork (64×64 source, existing pipeline) left, title bold, artist in green, album muted, thin rounded progress bar with elapsed/total under it.
- Mockups used during design: `.superpowers/brainstorm/*/content/estilo-pantallas.html` and `bicho-estilos.html` (option B in both) — reference for look, not pixel spec.

## Habitación screen + cat (decided with owner; details marked as my proposal were accepted)

Layout: header on top; cat centred in the middle area (~90×80 px); bottom strip (~56 px) with three values: **TEMP**, **HUMEDAD**, **CONFORT** (word). Cat must never overlap the strip.

Cat: small cat face/body drawn with TFT primitives — ears (triangles), round body, eyes, little nose/mouth, whiskers.

Colour by temperature (body colour; also used for the TEMP value and comfort word):
| Temp | Colour | Comfort word |
|---|---|---|
| < 18 °C | blue `#5ab0ff` | fresco |
| 18–25 °C | green `#4fd18b` | bien |
| > 25 °C | orange `#ff8a3d` | calor |

Thresholds go in `config.h`.

Moods / animation:
- **Awake idle**: blinks every few seconds, small ear twitch now and then.
- **Humidity face**: > 65 % → "sweaty" face; < 30 % → "dry" face (e.g. tongue/open mouth). Thresholds in `config.h`.
- **Sleeping**: 23:00–08:00 (NTP local time; hours in `config.h`). Cat curls into a ball, eyes closed, "Zzz" floating; darker tint of its temperature colour.
- **Woken by tap at night**: awake for a few seconds (~10 s), then back to sleep.
- **Petted (long press)**: happy face (^ ^ eyes) + small hop, ~2 s, then idle.

## Sensor behaviour
- DHT11 read every ~2 s or slower (sensor limit ~1 Hz). Keep last good value; ignore NaN reads.
- Until the first good read: show `--°` / `--%` and the cat in a neutral (green) colour.
- If no good read for a long time (builder picks, e.g. 30 s): show `--` again rather than stale values.

## Constraints
- Keep existing behaviour: tasks poll 5 s with SPIFFS offline cache, NTP, Spotify poll 3 s, artwork download.
- Network calls are blocking HTTP on the same loop; touch must still feel responsive. Builder decides how (e.g. checking touch between calls, short HTTP timeouts, or moving network polling to a FreeRTOS task on core 0).
- Avoid flicker: redraw only the cat region (sprite/off-screen buffer such as `TFT_eSprite` for the cat area is fine) and only changed text.
- Free RAM must still fit the 8 KB artwork buffer; a ~100×90 16-bit sprite (~18 KB) is acceptable on ESP32.
- Spanish UI text, as today.

## Left to the builder
- Code structure (new modules e.g. `sensors.*`, `touch.*`, `cat.*`, `ui_*`), state machine for screens/moods.
- Exact cat drawing, animation timing, debounce values, long-press handling details.
- Exact fonts / sizes within the style above.
- How the Spotify screen's progress updates without full redraw.
- Library: any maintained DHT library for PlatformIO (e.g. `adafruit/DHT sensor library`).
- Wiring notes section in README or spec for the two new modules.
