"""Cover-art proxy for the panel.

The panel decodes only baseline JPEGs that fit its buffer, at one fixed size.
Spotify and Last.fm covers come in other sizes/formats (Last.fm often PNG), so
the backend downloads the current cover once, converts it to an ART_PX x
ART_PX baseline JPEG and serves it at ARTWORK_PATH?v=<hash>. The hash changes
with the image, so the panel (and any CDN in front) never shows a stale one.
"""
import hashlib
import io
import logging
import os

import httpx
from PIL import Image

logger = logging.getLogger(__name__)

ARTWORK_PATH = "/nowplaying/artwork.jpg"
ART_PX = 120             # must match the firmware's ARTWORK_SIZE
ART_MAX_BYTES = 16384    # must fit the firmware's MAX_ARTWORK_BYTES

_src: str = ""           # source URL the stored JPEG came from
_art: bytes = b""
_hash: str = ""


def get_artwork() -> bytes:
    return _art


def to_panel_jpeg(raw: bytes) -> bytes:
    """Any image -> ART_PX square baseline JPEG within ART_MAX_BYTES, or b''."""
    try:
        img = Image.open(io.BytesIO(raw)).convert("RGB")
        img = img.resize((ART_PX, ART_PX), Image.LANCZOS)
    except Exception:  # corrupt / unsupported image
        return b""
    for quality in (88, 75, 60, 45):
        out = io.BytesIO()
        img.save(out, "JPEG", quality=quality, progressive=False)
        if out.tell() <= ART_MAX_BYTES:
            return out.getvalue()
    return b""


async def panel_url(src_url: str) -> str:
    """Panel artwork URL for this cover ('' when none / conversion failed)."""
    global _src, _art, _hash
    if src_url != _src:
        _src, _art, _hash = src_url, b"", ""
        if src_url:
            try:
                async with httpx.AsyncClient(timeout=8.0) as client:
                    resp = await client.get(src_url)
                    resp.raise_for_status()
                _art = to_panel_jpeg(resp.content)
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
    global _src, _art, _hash
    _src, _art, _hash = "", b"", ""
