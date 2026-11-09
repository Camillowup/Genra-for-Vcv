#!/usr/bin/env python3
"""Panel of GOLANA FX, the expander that docks right of GOLANA: the details of its three effects.

Usage:  mono_panel.py real                    -> res/GolanaFx.svg (text as paths) + src/golana/GolanaFxLayout.hpp
        mono_panel.py preview OUT.png [RACK]  -> preview with Rack components and the filter menu text
                                                 (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)
        mono_panel.py preview OUT.png RACK menu  -> the same with the filter menu open

Coordinates in mm (26 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the filter name in the dropdown field.
"""
import os
import re
import subprocess
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
MENU = PREVIEW and "menu" in sys.argv[3:]
PMENU = PREVIEW and "presets" in sys.argv[3:]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 and sys.argv[3] not in ("menu", "presets") else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 14
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/golana/Golana.cpp (ACCENT): the melody track's cyan (= FUNKE)
ACC, ACC2, INK = "#3dc9ff", "#2687b0", "#0c2a3a"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {400: "Inter-Regular.otf", 500: "Inter-Medium.otf", 600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf",
         800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps = [], []


def font(weight):
    if weight not in _fonts:
        f = TTFont(FONT_DIR + FONTS[weight])
        _fonts[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _fonts[weight]


def e(s):
    out.append(s)


def text(x, y, s, size=1.8, color=DIM, weight=600, anchor="middle", sp=0.25):
    gs, cmap, upm, hmtx = font(weight)
    scale = size / upm
    glyphs = [cmap.get(ord(c), "space") for c in s]
    w = sum(hmtx[g][0] * scale + sp for g in glyphs) - sp
    cx = x - (w / 2 if anchor == "middle" else (w if anchor == "end" else 0))
    paths = []
    for g in glyphs:
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, cx, y)))
        if pen.getCommands():
            paths.append(pen.getCommands())
        cx += hmtx[g][0] * scale + sp
    if paths:
        e(f'<path fill="{color}" d="{" ".join(paths)}"/>')


def tint(f):
    """The accent mixed onto the plate (f: the accent's share), as on the JIANT voices and MONK."""
    c = [int(ACC[i:i + 2], 16) for i in (1, 3, 5)]
    p = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(p[i] + (c[i] - p[i]) * f):02x}" for i in range(3))


def section(x, y, w, h, title):
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="{h}" rx="1.2" fill="{PLATE}"/>')
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="0.88" rx="0.44" fill="{ACC}"/>')
    text(x + w / 2, y + 5.0, title, 2.03, ACC, 800, "middle", 0.75)


def knob(name, x, y, label, kind="knob", hot=False):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x}" cy="{y}" r="{r:.2f}" fill="none" stroke="{ACC if hot else RING}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    text(x, y + R[kind] + 3.4, label, 1.7, TXT, 600, "middle", 0.3)


def switch(name, x, y, top, bot):
    comps.append(("switch", name, x, y))
    text(x, y - 4.5, top, 1.5, TXT, 600, "middle", 0.25)
    text(x, y + 5.9, bot, 1.5, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.25}" y="{y - 4.75}" width="8.5" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.63, TXT, 600, "middle", 0.3)


def adsr(x, y, w=10, h=2.6):
    e(f'<polyline points="{x},{y} {x + w * .18:.2f},{y - h} {x + w * .38:.2f},{y - h * .45:.2f} '
      f'{x + w * .75:.2f},{y - h * .45:.2f} {x + w},{y}" fill="none" stroke="{ACC}" stroke-width="0.35" '
      f'stroke-linejoin="round" stroke-opacity="0.7"/>')


def width(s, size, weight, sp):
    gs, cmap, upm, hmtx = font(weight)
    return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


# ---------------------------------------------------------------- helpers of this panel
fields = []   # (name, x, y, w, h, preview text, preview hint)


def field(name, x, y, w, h=5.4, val="", hint=""):
    """A small dropdown field: the module writes the value, click = list, wheel = step."""
    fields.append((name, x, y, w, h, val, hint))
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h}" rx="0.8" fill="{DARK}" stroke="{LINE}" '
      f'stroke-width="0.35"/>')
    cx, cy = x + w - 2.4, y + h / 2
    e(f'<polyline points="{cx - 0.9:.2f},{cy - 0.45:.2f} {cx:.2f},{cy + 0.5:.2f} {cx + 0.9:.2f},{cy - 0.45:.2f}" '
      f'fill="none" stroke="{ACC}" stroke-width="0.35" stroke-linecap="round" stroke-linejoin="round"/>')


def button(name, x, y, label):
    comps.append(("button", name, x, y))
    text(x, y + 4.6, label, 1.5, TXT, 600, "middle", 0.25)


def vline(x, y1, y2):
    e(f'<line x1="{x:.2f}" y1="{y1}" x2="{x:.2f}" y2="{y2}" stroke="{LINE}" stroke-width="0.3"/>')


# ---------------------------------------------------------------- frame and header (as K-WASUB FX)
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.4, 7.4, "GOLANA FX", 3.7, TITLE, 800, "start", 0.62)
# LINK: lit while docked right of a GOLANA (drawn by the module)
LINK = (W - 22.0, 6.95)
e(f'<circle cx="{LINK[0]}" cy="{LINK[1] - 0.6}" r="1.1" fill="#0d0e11"/>')

sx, sw = 3, W - 6
SH = 29


def cvstrip(x0, x1, y):
    """a darker strip behind a section's CV inputs, so they read apart from the knobs"""
    e(f'<rect x="{x0:.2f}" y="{y - 5.0:.2f}" width="{x1 - x0:.2f}" height="11.6" rx="0.9" fill="{BG}"/>')
    text(x0 + 1.4, y - 5.8, "CV", 1.3, DIM, 800, "start", 0.4)


def onbtn(name, y):
    """the section's on/off button in the corner (as on GRUND FX / K-WASUB FX)"""
    comps.append(("button", name, sx + 5.6, y + 10.2))


def block(y, title, fname, val, knobs, cvs, on):
    section(sx, y, sw, SH, title)
    onbtn(on, y)
    field(fname, sx + 11, y + 7.5, sw - 14, val=val)
    ry = y + 20.5
    for i, (n, l) in enumerate(knobs):
        knob(n, sx + 9 + i * 13, ry, l, "small")
    cvstrip(sx + sw - 3 - len(cvs) * 13 + 1, sx + sw - 2, ry)
    for i, (n, l) in enumerate(cvs):
        jack(n, sx + sw - 9 - (len(cvs) - 1 - i) * 13, ry, l)


block(11.0, "MOD FX", "F_MFX", "CHORUS", [("CRATE", "RATE"), ("CDEPTH", "DEPTH")],
      [("IN_CRATE", "RATE"), ("IN_CDEPTH", "DEPTH")], "MFX_ON")

# DELAY: two fields (type, time) and three knobs with their CVs
y = 11.0 + SH + 2
section(sx, y, sw, SH, "DELAY")
onbtn("DLY_ON", y)
hw = (sw - 14 - 1.5) / 2
field("F_DTYPE", sx + 11, y + 7.5, hw, val="STEREO")
field("F_DTIME", sx + 11 + hw + 1.5, y + 7.5, hw, val="1/8")
ry = y + 20.5
for i, (n, l) in enumerate([("DFDBK", "FDBK"), ("DCOLOR", "COLOR"), ("DMIX", "MIX")]):
    knob(n, sx + 6.5 + i * 10.3, ry, l, "small")
cvstrip(sx + 32.5, sx + sw - 1.5, ry)
for i, (n, l) in enumerate([("IN_DFDBK", "FDBK"), ("IN_DCOLOR", "COLOR"), ("IN_DMIX", "MIX")]):
    jack(n, sx + 38.3 + i * 10.2, ry, l)

block(11.0 + 2 * (SH + 2), "REVERB", "F_RTYPE", "HALL", [("RSIZE", "SIZE"), ("RDAMP", "DAMP")],
      [("IN_RSIZE", "SIZE"), ("IN_RDAMP", "DAMP")], "REV_ON")

# CHAIN: the order of the three
y = 11.0 + 3 * (SH + 2)
section(sx, y, sw, H - 3.5 - y, "CHAIN")
field("F_ROUTE", sx + 3, y + 10.0, sw - 22, val="MFX › DLY › REV")
cvstrip(sx + sw - 15.5, sx + sw - 2, y + 12.5)
jack("IN_ROUTE", sx + sw - 8.75, y + 12.5, "CHAIN")

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GolanaFx.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/golana_fx_panel.py - do not edit", "#pragma once", "namespace gfxlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, x, y, w, h, _, _ in fields:
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const float LINK[2] = {{{LINK[0]:.3f}f, {LINK[1]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "golana"), exist_ok=True)
    open(os.path.join(ROOT, "src", "golana", "GolanaFxLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GolanaFx.svg and src/golana/GolanaFxLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components, field texts, screen
_cache = {}


def img(name, x, y, rot=0.0):
    fn = os.path.join(CL, name + ".svg")
    if fn not in _cache:
        src = open(fn).read()
        src = re.sub(r"<\?xml[^>]*\?>|<!DOCTYPE[^>]*>|<!--.*?-->", "", src, flags=re.S)
        m = re.search(r"<svg[^>]*>", src, re.S)
        tag = m.group(0)
        w = float(re.search(r'\swidth="([0-9.]+)', tag).group(1))
        h = float(re.search(r'\sheight="([0-9.]+)', tag).group(1))
        vb = re.search(r'viewBox="([^"]+)"', tag)
        body = src[m.end():src.rindex("</svg>")]
        body = re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)|<metadata.*?</metadata>", "", body, flags=re.S)
        body = re.sub(r"</?(sodipodi|inkscape):[^>]*>", "", body)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w / 2.9528, h / 2.9528, body)
    vbs, sw, sh, body = _cache[fn]
    g = f'<g transform="rotate({rot} {x} {y})">' if rot else "<g>"
    out.insert(-1, f'{g}<svg x="{x - sw / 2:.3f}" y="{y - sh / 2:.3f}" width="{sw:.3f}" height="{sh:.3f}" '
                   f'viewBox="{vbs}" overflow="visible">{body}</svg></g>')


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


for kind, n, x, y in comps:
    if kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "switch":
        img("CKSS_1", x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx, sy)
for n, x, y, w, h, val, hint in fields:
    big = h > 5
    ptext(x + 1.8, y + h / 2 + (0.68 if big else 0.6), val, 1.9 if big else 1.6, TXT, 700, "start", 0.3)
    if hint:
        ptext(x + w - 4.6, y + h / 2 + 0.55, hint, 1.4, DIM, 600, "end", 0.25)
out.insert(-1, f'<circle cx="{LINK[0]}" cy="{LINK[1] - 0.6}" r="1.1" fill="{ACC}"/>')
ptext(LINK[0] + 2.0, LINK[1], "LINK", 1.5, TXT, 700, "start", 0.35)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
out[0] = out[0].replace("<svg ", '<svg xmlns:xlink="http://www.w3.org/1999/xlink" ', 1)
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2000", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
