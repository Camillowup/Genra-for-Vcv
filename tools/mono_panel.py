#!/usr/bin/env python3
"""Panel of GENRA MONO, the monophonic bass synth after the Moog Minitaur, in the GENRA look (accent: turquoise).

Usage:  mono_panel.py real                    -> res/GenraMono.svg (text as paths) + src/mono/MonoLayout.hpp
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import genra_strip  # noqa: E402

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

HP = 28
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/mono/GenraMono.cpp (ACCENT)
ACC, ACC2, INK = "#b8f23a", "#7da11f", "#2c3a0c"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}

# the filter types, in the order of the FTYPE parameter (keep in step with src/mono/MonoDsp.hpp)
FILTERS = [("LADDER 24", "Moog-style LP"), ("LADDER 12", "Moog-style LP, 2-pole"), ("DIODE", "303-style LP"),
           ("MS-20", "Sallen-Key LP"), ("SVF LP", "Oberheim SEM"), ("SVF BP", "Oberheim SEM"),
           ("SVF HP", "Oberheim SEM"), ("OTA", "Juno-style LP")]

# the factory presets by style, in list order (keep in step with src/mono/MonoPresets.hpp) - preview only
PRESETS = [("SUB", ["DEEP SUB", "DUB SUB", "808 GLIDE", "ROUND SUB"]),
           ("CLASSIC", ["TAURUS", "FAT MOOG", "FUNK PLUCK", "SYNTHWAVE"]),
           ("GARAGE · 2-STEP", ["ORGAN BASS", "2-STEP BOUNCE", "SPEED REESE"]),
           ("DUB · DUBSTEP · GRIME", ["REESE", "WOBBLE", "GRIME SQUARE"]),
           ("ACID", ["ACID 303", "SQUELCH", "RUBBER"]),
           ("HOUSE · TECHNO · ELECTRO", ["DEEP HOUSE", "MINIMAL BLIP", "ELECTRO ZAP"]),
           ("HIP-HOP · LO-FI", ["BOOM BAP", "LO-FI ROUND", "TRIP-HOP"])]

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


# ---------------------------------------------------------------- frame and header (as MONK and the JIANT voices)
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "MONO", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("MONO", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · TAURUS-STYLE BASS", 1.76, DIM, 600, "start", 0.47)
e(f'<rect x="{W - 27.1:.2f}" y="5.55" width="11.5" height="1.08" rx="0.54" fill="{ACC}"/>')
# the preset selector in the header: the module writes number and name (+ a dot when edited), click = list, wheel = step
PFIELD = (77.5, 3.3, 34.0, 5.4)
e(f'<rect x="{PFIELD[0]}" y="{PFIELD[1]}" width="{PFIELD[2]}" height="{PFIELD[3]}" rx="0.9" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')
pcx = PFIELD[0] + PFIELD[2] - 3
e(f'<polyline points="{pcx - 1.2:.2f},{PFIELD[1] + 2.1} {pcx:.2f},{PFIELD[1] + 3.4} {pcx + 1.2:.2f},{PFIELD[1] + 2.1}" '
  f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')

# ---------------------------------------------------------------- oscillators: per VCO wave, tune, level; glide
ow = 65.16
section(3, 13.5, ow, 45.5, "OSCILLATORS")
for row, y in ((1, 27.0), (2, 45.0)):
    text(5.6, y + 0.7, f"VCO{row}", 1.7, ACC, 800, "start", 0.3)
    knob(f"WAVE{row}", 21, y, "WAVE")
    knob(f"TUNE{row}", 35, y, "TUNE")
    knob(f"LEVEL{row}", 49, y, "LEVEL")
e(f'<line x1="56.5" y1="21" x2="56.5" y2="55" stroke="{LINE}" stroke-width="0.3"/>')
knob("GLIDE", 62.3, 27.5, "GLIDE", "small")
knob("ACCENT", 62.3, 45.5, "ACCENT", "small")

# ---------------------------------------------------------------- filter
fx = 3 + ow + 2
fw = W - 3 - fx
section(fx, 13.5, fw, 45.5, "FILTER")
FIELD = (fx + 3, 20.6, fw - 6, 5.4)
e(f'<rect x="{FIELD[0]:.2f}" y="{FIELD[1]}" width="{FIELD[2]:.2f}" height="{FIELD[3]}" rx="0.9" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')
cx = FIELD[0] + FIELD[2] - 3
e(f'<polyline points="{cx - 1.2:.2f},{FIELD[1] + 2.1} {cx:.2f},{FIELD[1] + 3.4} {cx + 1.2:.2f},{FIELD[1] + 2.1}" '
  f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')
knob("CUTOFF", fx + 16, 40.5, "CUTOFF", "large", hot=True)
knob("RES", fx + 38, 35, "RES")
knob("EGAMT", fx + 57, 35, "EG AMT")
knob("DRIVE", fx + 38, 50, "DRIVE", "small")
knob("KEYTRK", fx + 57, 50, "KEY TRK", "small")

# ---------------------------------------------------------------- envelopes
mid = W / 2
section(3, 61, W - 6, 25, "ENVELOPES")
text(mid / 2 + 2, 70.0, "FILTER", 1.6, TXT, 800, "middle", 0.4)
adsr(mid / 2 + 8.5, 70.2)
text(mid + mid / 2 - 2, 70.0, "AMP", 1.6, TXT, 800, "middle", 0.4)
adsr(mid + mid / 2 + 2.5, 70.2)
for i, l in enumerate("ADSR"):
    knob(f"F{l}", 14 + i * 15, 76.3, l, "small")
    knob(f"A{l}", mid + 12.1 + i * 15, 76.3, l, "small")
e(f'<line x1="{mid:.2f}" y1="68" x2="{mid:.2f}" y2="83" stroke="{LINE}" stroke-width="0.3"/>')

# ---------------------------------------------------------------- lfo
lw = 98
section(3, 88, lw, 17.5, "LFO")
knob("RATE", 13, 97.6, "RATE", "small")
switch("LFOWAVE", 27, 97.6, "TRI", "SQR")
knob("LFOPITCH", 40, 97.6, "> PITCH", "small")
knob("LFOFILT", 64, 97.6, "> FILTER", "small")
e(f'<line x1="72.5" y1="92" x2="72.5" y2="103.5" stroke="{LINE}" stroke-width="0.3"/>')
jack("IN_RATE", 80, 97.0, "RATE")
jack("IN_VCA", 93, 97.0, "VCA")
section(3 + lw + 2, 88, W - 3 - (3 + lw + 2), 17.5, "STEREO")
knob("WIDTH", (3 + lw + 2 + W - 3) / 2, 97.6, "WIDTH", "small")

# ---------------------------------------------------------------- in / out
e(f'<rect x="3" y="107.5" width="{W - 6:.2f}" height="18" rx="1.2" fill="{PLATE}"/>')
# the GENRA strip (as on every instrument), then the CV inputs
x = genra_strip(jack, e, 4.5, 114.6, {"V/OCT": "IN_VOCT", "GATE": "IN_GATE", "VEL": "IN_VEL", "ACC": "IN_ACC",
                                      "SLIDE": "IN_SLIDE"}, ACC, {"CLOCK": "IN_SYNC", "RESET": "IN_RETRIG"})
e(f'<line x1="{x + 1.6:.2f}" y1="109.5" x2="{x + 1.6:.2f}" y2="123.5" stroke="{LINE}" stroke-width="0.3"/>')
for i, (n, l) in enumerate([("IN_CUTOFF", "CUT"), ("IN_RES", "RES"), ("IN_DRIVE", "DRIVE"), ("IN_EXT", "EXT")]):
    jack(n, x + 7.9 + i * 9.4, 114.6, l)
e(f'<line x1="{W - 31:.2f}" y1="109.5" x2="{W - 31:.2f}" y2="123.5" stroke="{LINE}" stroke-width="0.3"/>')
jack("OUT_L", W - 23, 114.6, "L", True)
jack("OUT_R", W - 11, 114.6, "R", True)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GenraMono.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/mono_panel.py - do not edit", "#pragma once", "namespace mlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    lines.append(f"static const float FIELD[4] = {{{FIELD[0]:.3f}f, {FIELD[1]:.3f}f, {FIELD[2]:.3f}f, {FIELD[3]:.3f}f}};")
    lines.append(f"static const float PFIELD[4] = {{{PFIELD[0]:.3f}f, {PFIELD[1]:.3f}f, {PFIELD[2]:.3f}f, {PFIELD[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "mono"), exist_ok=True)
    open(os.path.join(ROOT, "src", "mono", "MonoLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GenraMono.svg and src/mono/MonoLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components + dropdown text
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
    else:
        img("PJ301M", x, y)
for sx in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx, sy)
ptext(FIELD[0] + 2, FIELD[1] + 3.55, FILTERS[0][0], 1.9, TXT, 700, "start", 0.35)
ptext(FIELD[0] + 20.5, FIELD[1] + 3.55, FILTERS[0][1].upper(), 1.5, DIM, 600, "start", 0.3)
if MENU:
    y0 = FIELD[1] + FIELD[3] + 0.4
    out.insert(-1, f'<rect x="{FIELD[0]:.2f}" y="{y0}" width="{FIELD[2]}" height="{len(FILTERS) * 5 + 1}" rx="0.9" '
                   f'fill="#101115" stroke="{ACC}" stroke-width="0.35"/>')
    for i, (nm, d) in enumerate(FILTERS):
        y = y0 + 0.5 + i * 5
        if i == 0:
            out.insert(-1, f'<rect x="{FIELD[0] + 0.5}" y="{y}" width="{FIELD[2] - 1}" height="5" rx="0.6" '
                           f'fill="{ACC}" fill-opacity="0.22"/>')
        ptext(FIELD[0] + 2.5, y + 3.4, nm, 1.8, ACC if i == 0 else TXT, 700, "start", 0.3)
        ptext(FIELD[0] + FIELD[2] - 2, y + 3.4, d, 1.5, DIM, 500, "end", 0.2)
ptext(PFIELD[0] + 2, PFIELD[1] + 3.55, "01", 1.6, ACC, 700, "start", 0.3)
ptext(PFIELD[0] + 6.2, PFIELD[1] + 3.55, PRESETS[0][1][0], 1.9, TXT, 700, "start", 0.35)
if PMENU:
    y0 = PFIELD[1] + PFIELD[3] + 0.4
    rows = sum(1 + len(n) for _, n in PRESETS)
    mw = 46
    out.insert(-1, f'<rect x="{PFIELD[0]:.2f}" y="{y0}" width="{mw}" height="{rows * 3.7 + 1}" rx="0.9" '
                   f'fill="#101115" stroke="{ACC}" stroke-width="0.35"/>')
    y, k = y0 + 0.5, 0
    for style, names in PRESETS:
        ptext(PFIELD[0] + 2, y + 2.7, style, 1.35, ACC, 800, "start", 0.45)
        y += 3.7
        for nm in names:
            k += 1
            if k == 1:
                out.insert(-1, f'<rect x="{PFIELD[0] + 0.5}" y="{y}" width="{mw - 1}" height="3.7" rx="0.5" '
                               f'fill="{ACC}" fill-opacity="0.22"/>')
            ptext(PFIELD[0] + 3, y + 2.6, f"{k:02d}", 1.4, DIM, 600, "start", 0.2)
            ptext(PFIELD[0] + 7.5, y + 2.6, nm, 1.6, ACC if k == 1 else TXT, 700, "start", 0.3)
            y += 3.7
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1100", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
