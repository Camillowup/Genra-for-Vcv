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

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
MENU = PREVIEW and "menu" in sys.argv[3:]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 and sys.argv[3] != "menu" else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 26
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
# keep in step with src/mono/GenraMono.cpp (ACCENT)
ACC, ACC2, INK = "#1fb3a3", "#0e7a70", "#06302b"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}

# the filter types, in the order of the FTYPE parameter (keep in step with src/mono/MonoDsp.hpp)
FILTERS = [("LADDER 24", "Moog-style LP"), ("LADDER 12", "Moog-style LP, 2-pole"), ("DIODE", "303-style LP"),
           ("MS-20", "Sallen-Key LP"), ("SVF LP", "Oberheim SEM"), ("SVF BP", "Oberheim SEM"),
           ("SVF HP", "Oberheim SEM"), ("OTA", "Juno-style LP")]

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


def section(x, y, w, h, title):
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="{h}" rx="1.6" fill="{PLATE}"/>')
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="0.9" rx="0.45" fill="{ACC}"/>')
    text(x + 2.5, y + 4.8, title, 2.2, ACC, 800, "start", 0.5)


def knob(name, x, y, label, kind="knob", hot=False):
    comps.append((kind, name, x, y))
    r = R[kind] + 1.0
    e(f'<circle cx="{x}" cy="{y}" r="{r:.2f}" fill="none" stroke="{ACC if hot else LINE}" '
      f'stroke-opacity="{0.6 if hot else 1}" stroke-width="0.35" stroke-dasharray="0.4 0.9"/>')
    text(x, y + R[kind] + 3.5, label, 1.75)


def switch(name, x, y, top, bot):
    comps.append(("switch", name, x, y))
    text(x, y - 4.5, top, 1.5)
    text(x, y + 5.9, bot, 1.5)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.7}" y="{y - 4.9}" width="9.4" height="11.9" rx="1.2" fill="{ACC}"/>')
    text(x, y + 5.6, label, 1.55, INK if out_ else DIM, 800 if out_ else 600, "middle", 0.2)


def adsr(x, y, w=10, h=2.6):
    e(f'<polyline points="{x},{y} {x + w * .18:.2f},{y - h} {x + w * .38:.2f},{y - h * .45:.2f} '
      f'{x + w * .75:.2f},{y - h * .45:.2f} {x + w},{y}" fill="none" stroke="{ACC}" stroke-width="0.35" '
      f'stroke-linejoin="round" stroke-opacity="0.7"/>')


# ---------------------------------------------------------------- frame and header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
e(f'<rect width="{W:.2f}" height="12" fill="{ACC}"/>')
e(f'<rect y="12" width="{W:.2f}" height="0.8" fill="{ACC2}"/>')
text(14, 8.4, "GENRA", 4.2, BG, 800, "start", 0.8)
text(34.6, 8.4, "MONO", 4.2, BG, 400, "start", 0.8)
text(W - 14, 8.1, "TAURUS-STYLE BASS SYNTH", 1.6, INK, 700, "end", 0.35)

# ---------------------------------------------------------------- oscillators
section(3, 15, 55, 44, "OSCILLATORS")
xs = [12.5, 30.5, 48.5]
for x, (n, l) in zip(xs, [("WAVE1", "VCO1 WAVE"), ("WAVE2", "VCO2 WAVE"), ("FREQ2", "VCO2 FREQ")]):
    knob(n, x, 27.5, l)
for x, (n, l) in zip(xs, [("LEVEL1", "VCO1 LVL"), ("LEVEL2", "VCO2 LVL"), ("GLIDE", "GLIDE")]):
    knob(n, x, 46, l)

# ---------------------------------------------------------------- filter
fw = W - 3 - 60
section(60, 15, fw, 44, "FILTER")
e(f'<path d="M110,19.6 H120 Q122.5,19.6 123.3,17.2 Q124.2,20.4 126,22.2" fill="none" stroke="{ACC}" '
  f'stroke-width="0.35" stroke-opacity="0.7"/>')
FIELD = (63, 22.2, 63, 5.4)
e(f'<rect x="{FIELD[0]}" y="{FIELD[1]}" width="{FIELD[2]}" height="{FIELD[3]}" rx="0.9" fill="{DARK}" '
  f'stroke="{ACC}" stroke-width="0.35"/>')
e(f'<polyline points="121.6,24.2 122.8,25.5 124,24.2" fill="none" stroke="{ACC}" stroke-width="0.4" '
  f'stroke-linecap="round" stroke-linejoin="round"/>')
knob("CUTOFF", 76, 41.5, "CUTOFF", "large", hot=True)
knob("RES", 98, 36, "RES")
knob("EGAMT", 117, 36, "EG AMT")
knob("DRIVE", 98, 50.5, "DRIVE", "small")
knob("KEYTRK", 117, 50.5, "KEY TRK", "small")

# ---------------------------------------------------------------- envelopes
section(3, 61, W - 6, 25, "ENVELOPES")
text(34, 69.4, "FILTER", 1.6, TXT, 800, "middle", 0.4)
adsr(40.5, 69.6)
text(97, 69.4, "AMP", 1.6, TXT, 800, "middle", 0.4)
adsr(101.5, 69.6)
for i, l in enumerate("ADSR"):
    knob(f"F{l}", 13 + i * 14, 75.8, l, "small")
    knob(f"A{l}", 77 + i * 14, 75.8, l, "small")
e(f'<line x1="66.04" y1="66" x2="66.04" y2="83" stroke="{LINE}" stroke-width="0.3"/>')

# ---------------------------------------------------------------- lfo
section(3, 88, W - 6, 17, "LFO")
knob("RATE", 34, 96.2, "RATE", "small")
switch("LFOWAVE", 54, 96.2, "TRI", "SQR")
knob("LFOPITCH", 76, 96.2, "> PITCH", "small")
knob("LFOFILT", 100, 96.2, "> FILTER", "small")

# ---------------------------------------------------------------- in / out
e(f'<rect x="3" y="107" width="{W - 6:.2f}" height="18.5" rx="1.6" fill="{PLATE}"/>')
for i, (n, l) in enumerate([("IN_VOCT", "V/OCT"), ("IN_GATE", "GATE"), ("IN_VEL", "VEL"), ("IN_CUTOFF", "CUTOFF"),
                            ("IN_RES", "RES"), ("IN_DRIVE", "DRIVE"), ("IN_RATE", "RATE"), ("IN_VCA", "VCA"),
                            ("IN_EXT", "EXT")]):
    jack(n, 9.5 + i * 10.4, 114.3, l)
e(f'<line x1="106.4" y1="109" x2="106.4" y2="123.5" stroke="{LINE}" stroke-width="0.3"/>')
jack("OUT_LFO", 112, 114.3, "LFO", True)
jack("OUT_AUDIO", 123, 114.3, "OUT", True)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GenraMono.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/mono_panel.py - do not edit", "#pragma once", "namespace mlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    lines.append(f"static const float FIELD[4] = {{{FIELD[0]:.3f}f, {FIELD[1]:.3f}f, {FIELD[2]:.3f}f, {FIELD[3]:.3f}f}};")
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
        r = R[kind] * 0.82
        out.insert(-1, f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - r:.2f}" stroke="{ACC}" stroke-width="0.7" stroke-linecap="round"/>')
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
    out.insert(-1, f'<rect x="{FIELD[0]}" y="{y0}" width="{FIELD[2]}" height="{len(FILTERS) * 5 + 1}" rx="0.9" '
                   f'fill="#101115" stroke="{ACC}" stroke-width="0.35"/>')
    for i, (nm, d) in enumerate(FILTERS):
        y = y0 + 0.5 + i * 5
        if i == 0:
            out.insert(-1, f'<rect x="{FIELD[0] + 0.5}" y="{y}" width="{FIELD[2] - 1}" height="5" rx="0.6" '
                           f'fill="{ACC}" fill-opacity="0.22"/>')
        ptext(FIELD[0] + 2.5, y + 3.4, nm, 1.8, ACC if i == 0 else TXT, 700, "start", 0.3)
        ptext(FIELD[0] + FIELD[2] - 2, y + 3.4, d, 1.5, DIM, 500, "end", 0.2)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1100", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
