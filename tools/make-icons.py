#!/usr/bin/env python3
"""Generate square monochrome tile icons (res/icons/<slug>.png) for the grid.

Each icon is a 200x200 PNG: rounded square outline with a short glyph. The
slug matches the NickelMenu label (lowercase, runs of non-alphanumerics -> '-').
Run on Windows: python tools/make-icons.py
"""
import os, re
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "res", "icons")
SIZE = 200
FONTS = [r"C:\Windows\Fonts\seguisb.ttf", r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\arialbd.ttf"]
SYMBOL_FONTS = [r"C:\Windows\Fonts\seguisym.ttf", r"C:\Windows\Fonts\segoeui.ttf"]

def slug(label):
    return re.sub(r"[^a-z0-9]+", "-", label.lower()).strip("-")

def font(paths, size):
    for p in paths:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

# label -> (glyph, symbol?)  Glyph text drawn centered; symbol uses the symbol font.
ICONS = {
    "KOReader": ("KO", False),
    "Plato": ("Pl", False),
    "Cobalt": ("Co", False),
    "Kobold": ("Kb", False),
    "USB connection": ("USB", False),
    "Reboot": ("\u21bb", True),
    "Power off": ("\u23fb", True),
    "kpm - Check for updates": ("\u2713", True),
    "kpm - Update all": ("\u2b06", True),
    "kpm - Status": ("i", False),
    "kpm - Package manager": ("kpm", False),
    "Continue reading": ("\u25b6", True),
    "Rescan books": ("\u2b6e", True),
    "Tiles settings": ("\u2699", True),
}

os.makedirs(OUT, exist_ok=True)
for label, (glyph, symbol) in ICONS.items():
    img = Image.new("L", (SIZE, SIZE), 255)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([6, 6, SIZE - 7, SIZE - 7], radius=28, outline=0, width=7)
    fsize = 88 if len(glyph) <= 2 else 64
    f = font(SYMBOL_FONTS if symbol else FONTS, fsize)
    # Fallback for symbol glyphs the font cannot draw
    if symbol and f.getbbox(glyph)[2] - f.getbbox(glyph)[0] < 4:
        f = font(FONTS, 88); glyph = label[0]
    box = f.getbbox(glyph)
    w, h = box[2] - box[0], box[3] - box[1]
    d.text(((SIZE - w) / 2 - box[0], (SIZE - h) / 2 - box[1]), glyph, fill=0, font=f)
    path = os.path.join(OUT, slug(label) + ".png")
    img.save(path, optimize=True)
    print(path, os.path.getsize(path))
