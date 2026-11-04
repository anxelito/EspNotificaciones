"""Screenshots of the Habitacion screen, rendered by the real firmware code.

Compiles the UI drawing code (room_scene, cat_draw, screen_room, chrome,
fonts) for the PC against a small TFT_eSPI shim, runs it, and turns the
frames into PNG stills, animated GIFs and a framed hero image.

    python firmware/tools/snapshots/snap.py [--out docs/habitacion]

Needs g++ (WinLibs/MinGW on Windows) and Pillow. The TFT_eSPI library must
be installed (run `pio run` once): its 5x7 font is reused.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
FW = os.path.normpath(os.path.join(HERE, "..", ".."))
ROOT = os.path.dirname(FW)
WINLIBS = os.path.expandvars(
    r"%LOCALAPPDATA%\Microsoft\WinGet\Packages"
    r"\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin")

SOURCES = [
    os.path.join(HERE, "snap.cpp"),
    os.path.join(HERE, "shim", "tft_host.cpp"),
    *[os.path.join(FW, "src", "ui", f + ".cpp")
      for f in ("room_scene", "cat_draw", "screen_room", "chrome", "text_fit", "fonts")],
    *[os.path.join(FW, "src", "logic", f + ".cpp") for f in ("room", "cat_state", "cat_behavior")],
]

STILLS = ["dia", "noche", "frio", "calor", "carino", "tarea"]
SCALE = 2


def compiler():
    gxx = shutil.which("g++")
    if gxx:
        return gxx, None
    cand = os.path.join(WINLIBS, "g++.exe")
    if os.path.exists(cand):
        return cand, WINLIBS
    sys.exit("g++ not found (install WinLibs or add it to PATH)")


def build(tmp):
    gxx, bindir = compiler()
    env = dict(os.environ)
    if bindir:
        env["PATH"] = bindir + os.pathsep + env["PATH"]
    exe = os.path.join(tmp, "snap.exe")
    fonts = os.path.join(FW, ".pio", "libdeps", "esp32dev", "TFT_eSPI", "Fonts")
    if not os.path.exists(os.path.join(fonts, "glcdfont.c")):
        sys.exit("TFT_eSPI not installed: run `pio run` in firmware/ once")
    cmd = [gxx, "-std=gnu++17", "-O1", "-w", "-I" + os.path.join(HERE, "shim"),
           "-I" + os.path.join(FW, "src", "ui"), "-I" + os.path.join(FW, "src"),
           "-I" + fonts, *SOURCES, "-o", exe]
    subprocess.run(cmd, check=True, env=env)
    return exe, env


def load(path):
    return Image.open(path).convert("RGB")


def big(img):
    return img.resize((img.width * SCALE, img.height * SCALE), Image.NEAREST)


def framed(img, pad=28, radius=26):
    """Screen inside a dark rounded bezel, like the real panel."""
    s = big(img)
    w, h = s.width + 2 * pad, s.height + 2 * pad
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(out)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius, fill=(18, 20, 27, 255))
    d.rounded_rectangle([6, 6, w - 7, h - 7], radius - 6, fill=(6, 7, 11, 255))
    out.paste(s, (pad, pad))
    return out


def gif(frames, path, ms):
    imgs = [big(load(f)).convert("P", palette=Image.ADAPTIVE, colors=128) for f in frames]
    imgs[0].save(path, save_all=True, append_images=imgs[1:], duration=ms, loop=0, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "habitacion"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        exe, env = build(tmp)
        frames = os.path.join(tmp, "frames")
        os.makedirs(frames)
        subprocess.run([exe, frames], check=True, env=env)

        for name in STILLS:
            big(load(os.path.join(frames, name + ".ppm"))).save(os.path.join(args.out, name + ".png"))
        framed(load(os.path.join(frames, "dia.ppm"))).save(os.path.join(args.out, "hero.png"))
        gif(sorted(glob.glob(os.path.join(frames, "anim_dia_*.ppm"))), os.path.join(args.out, "dia.gif"), 100)
        gif(sorted(glob.glob(os.path.join(frames, "anim_noche_*.ppm"))), os.path.join(args.out, "noche.gif"), 100)
    print("screenshots in", args.out)


if __name__ == "__main__":
    main()
