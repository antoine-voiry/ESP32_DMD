#!/usr/bin/env python3
"""Turns the raw frames from preview.cpp into LED-matrix looking GIF/PNG files for docs/img."""
import glob
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

CELL = 6          # pixels per LED
DOT = 2.3         # LED radius
OFF = (18, 18, 22)  # unlit LED
BACK = (6, 6, 8)


def led(frame):
    w, h = frame.size
    img = Image.new("RGB", (w * CELL, h * CELL), BACK)
    draw = ImageDraw.Draw(img)
    px = frame.load()
    for y in range(h):
        for x in range(w):
            c = px[x, y]
            cx, cy = x * CELL + CELL / 2, y * CELL + CELL / 2
            draw.ellipse((cx - DOT, cy - DOT, cx + DOT, cy + DOT), fill=c if sum(c) > 24 else OFF)
    # Soft glow: blurred copy added on top, like a real panel behind a diffuser.
    glow = img.filter(ImageFilter.GaussianBlur(CELL * 0.8))
    return Image.blend(img, Image.composite(glow, img, glow.convert("L")), 0.35)


def main():
    frames_dir, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    for name in ("fireworks", "plasma", "stars", "matrix"):
        files = sorted(glob.glob(os.path.join(frames_dir, name + "_*.ppm")))
        if not files:
            continue
        frames = [led(Image.open(f).convert("RGB")) for f in files]
        # One shared palette keeps GIFs small and avoids per-frame colour flicker.
        palette = frames[len(frames) // 2].convert("P", palette=Image.ADAPTIVE, colors=64)
        frames = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]
        frames[0].save(os.path.join(out_dir, name + ".gif"), save_all=True, append_images=frames[1:],
                       duration=40, loop=0, optimize=True, disposal=1)
    for name in ("icons_day", "wind", "tempo"):
        path = os.path.join(frames_dir, name + ".ppm")
        if os.path.exists(path):
            led(Image.open(path).convert("RGB")).save(os.path.join(out_dir, name + ".png"), optimize=True)
    print("previews written to", out_dir)


if __name__ == "__main__":
    main()
