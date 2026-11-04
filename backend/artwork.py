"""Now-playing artwork for the panel: one composed image per song.

The panel decodes only baseline JPEGs that fit its buffer. So the backend
downloads the current cover once and composes the whole music-screen
backdrop server-side (PANEL_W x PANEL_H, the panel body below its header):

    - the cover blurred, darkened and stretched over the whole image,
    - the sharp cover (COVER_PX square, soft shadow) centred at the top,
    - a fade into a solid dark colour (``bg``) where the panel draws title,
      artist, equaliser and progress bar, so its smooth text blends cleanly.

Served at ARTWORK_PATH?v=<hash>; the hash changes with the image, so the panel
(and any CDN in front) never shows a stale one. ``bg`` and ``accent`` are sent
with the now-playing JSON.
"""
import hashlib
import io
import logging
import os

import httpx
from PIL import Image, ImageDraw, ImageFilter

logger = logging.getLogger(__name__)

ARTWORK_PATH = "/nowplaying/artwork.jpg"
PANEL_W, PANEL_H = 320, 200   # must match the firmware's MUSIC_IMG_W / _H
COVER_PX = 120                # must match the firmware's ARTWORK_SIZE
COVER_Y = 6                   # cover top inside the image
TEXT_Y = 130                  # from here down the image is solid `bg`
ART_MAX_BYTES = 16000         # must fit the firmware's MAX_ARTWORK_BYTES

_src: str = ""           # source URL the stored JPEG came from
_art: bytes = b""
_hash: str = ""
_accent: str = ""        # "#rrggbb" main colour of the cover
_bg: str = ""            # "#rrggbb" solid colour under the panel's text


def get_artwork() -> bytes:
    return _art


def get_accent() -> str:
    return _accent


def get_bg() -> str:
    return _bg


def _hex(rgb) -> str:
    return "#%02x%02x%02x" % tuple(int(v) for v in rgb[:3])


def accent_rgb(img: Image.Image):
    """Main colour of an image: the most common of a few quantised colours,
    preferring saturated ones over grey/black/white."""
    small = img.convert("RGB").resize((32, 32))
    pal = small.quantize(colors=6, method=Image.Quantize.MEDIANCUT)
    palette = pal.getpalette()[:18]
    best, best_score = None, -1.0
    for count, idx in pal.getcolors():
        r, g, b = palette[idx * 3: idx * 3 + 3]
        mx, mn = max(r, g, b), min(r, g, b)
        sat = (mx - mn) / mx if mx else 0.0
        light = mx / 255.0
        score = count * (0.25 + sat) * (0.3 + min(light, 1.0 - light * 0.5))
        if score > best_score:
            best, best_score = (r, g, b), score
    return best


def accent_colour(raw: bytes) -> str:
    try:
        return _hex(accent_rgb(Image.open(io.BytesIO(raw))))
    except Exception:
        return ""


def _dark_bg(accent) -> tuple:
    """Text background: the accent pulled most of the way to black."""
    return tuple(int(c * 0.22 + 8) for c in accent)


def compose_panel(raw: bytes):
    """Cover bytes -> (jpeg, bg_hex, accent_hex), or (b'', '', '')."""
    try:
        cover = Image.open(io.BytesIO(raw)).convert("RGB")
    except Exception:  # corrupt / unsupported image
        return b"", "", ""
    acc = accent_rgb(cover)
    bg = _dark_bg(acc)

    # Blurred, darkened backdrop filling the whole image.
    w, h = cover.size
    side = min(w, h)
    sq = cover.crop(((w - side) // 2, (h - side) // 2, (w + side) // 2, (h + side) // 2))
    back = sq.resize((PANEL_W, PANEL_W), Image.BILINEAR).crop(
        (0, (PANEL_W - PANEL_H) // 2, PANEL_W, (PANEL_W + PANEL_H) // 2))
    back = back.filter(ImageFilter.GaussianBlur(14))
    back = Image.blend(back, Image.new("RGB", back.size, bg), 0.45)

    # Fade into solid bg below the cover (where the panel draws its text).
    fade = Image.new("L", (PANEL_W, PANEL_H), 0)
    d = ImageDraw.Draw(fade)
    fade_top = TEXT_Y - 34
    for y in range(fade_top, PANEL_H):
        a = 255 if y >= TEXT_Y else int(255 * (y - fade_top) / (TEXT_Y - fade_top))
        d.line([(0, y), (PANEL_W, y)], fill=a)
    back = Image.composite(Image.new("RGB", back.size, bg), back, fade)

    # Soft shadow + sharp cover.
    cx = (PANEL_W - COVER_PX) // 2
    shadow = Image.new("L", (PANEL_W, PANEL_H), 0)
    ImageDraw.Draw(shadow).rounded_rectangle(
        (cx - 2, COVER_Y + 3, cx + COVER_PX + 2, COVER_Y + COVER_PX + 6), 8, fill=170)
    shadow = shadow.filter(ImageFilter.GaussianBlur(6))
    back = Image.composite(Image.new("RGB", back.size, (0, 0, 0)), back, shadow)
    back.paste(sq.resize((COVER_PX, COVER_PX), Image.LANCZOS), (cx, COVER_Y))

    for quality in (85, 75, 65, 55, 45):
        out = io.BytesIO()
        back.save(out, "JPEG", quality=quality, progressive=False)
        if out.tell() <= ART_MAX_BYTES:
            return out.getvalue(), _hex(bg), _hex(acc)
    return b"", "", ""


async def panel_url(src_url: str) -> str:
    """Panel artwork URL for this cover ('' when none / conversion failed)."""
    global _src, _art, _hash, _accent, _bg
    if src_url != _src:
        _src, _art, _hash, _accent, _bg = src_url, b"", "", "", ""
        if src_url:
            try:
                async with httpx.AsyncClient(timeout=8.0) as client:
                    resp = await client.get(src_url)
                    resp.raise_for_status()
                _art, _bg, _accent = compose_panel(resp.content)
                _hash = hashlib.sha1(_art).hexdigest()[:10] if _art else ""
            except httpx.HTTPError as exc:
                logger.warning("artwork fetch failed: %s", exc)
                _src = ""          # retry on the next poll
    if not _hash:
        return ""
    base = os.getenv("BASE_URL", "").rstrip("/")
    return f"{base}{ARTWORK_PATH}?v={_hash}"


def reset() -> None:
    """Test helper."""
    global _src, _art, _hash, _accent, _bg
    _src, _art, _hash, _accent, _bg = "", b"", "", "", ""
