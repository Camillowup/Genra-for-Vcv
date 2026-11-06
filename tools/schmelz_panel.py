#!/usr/bin/env python3
"""Panel of SCHMELZ, the string machine (strings + solo section), in the GENRA look (accent: rose).

Usage:  schmelz_panel.py real                   -> res/Schmelz.svg (text as paths) + src/schmelz/SchmelzLayout.hpp
        schmelz_panel.py preview OUT.png [RACK] -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (32 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display (registration, solo sound, voices).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import genra_strip, _tray, SLOT  # noqa: E402

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 32
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/schmelz/Schmelz.cpp (ACCENT)
ACC = "#e0779e"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0, "button": 3.6}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}


FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {400: "Inter-Regular.otf", 500: "Inter-Medium.otf", 600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf",
         800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps, extra = [], [], {}


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


def width(s, size, weight, sp):
    gs, cmap, upm, hmtx = font(weight)
    return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


def tint(f):
    """The accent mixed onto the plate (f: the accent's share), as on GENRA MONO, MONK and the JIANT voices."""
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


def button(name, x, y, label):
    comps.append(("button", name, x, y))
    text(x, y + R["button"] + 3.0, label, 1.7, TXT, 600, "middle", 0.3)


def lamps(name, x0, dx, y, names, label):
    """A lamp row for a switch parameter; the module places a LampRow at (x0, y), dx apart."""
    extra[name] = (x0, y, dx)
    text(x0 - 5.0, y + 0.65, label, 1.6, DIM, 700, "end", 0.3)
    for i, n in enumerate(names):
        e(f'<circle cx="{x0 + i * dx:.2f}" cy="{y}" r="1.35" fill="#0d0e11"/>')
        text(x0 + i * dx, y + 4.4, n, 1.5, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.6}" y="{y - 4.75}" width="9.2" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.6, TXT, 600, "middle", 0.25)


# ---------------------------------------------------------------- frame, header, display
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "SCHMELZ", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("SCHMELZ", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · STRING MACHINE", 1.76, DIM, 600, "start", 0.47)
e(f'<rect x="{W - 27.1:.2f}" y="5.55" width="11.5" height="1.08" rx="0.54" fill="{ACC}"/>')

DISPLAY = (3, 10.5, W - 6, 14)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]:.2f}" height="{DISPLAY[3]}" rx="1.6" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')

# ---------------------------------------------------------------- strings
section(3, 26.5, 77, 52, "STRINGS")
knob("REG", 16, 45, "REGISTRATION", "large", hot=True)
knob("CRESC", 37, 42.5, "CRESC")
knob("REL", 52, 42.5, "RELEASE")
knob("TONE", 67, 42.5, "TONE")
knob("ENSDEPTH", 16, 68.5, "ENS DEPTH", "small")
lamps("OCT", 42, 11, 58.5, ["BASE", "BOTH", "8VA"], "OCT")
lamps("ENS", 40, 10.5, 68.5, ["OFF", "ENS", "E+C", "CHO"], "ENS")

# ---------------------------------------------------------------- solo
section(82, 26.5, W - 85, 52, "SOLO")
knob("SOUND", 96, 45, "SOUND", "large", hot=True)
knob("DECAY", 117, 42.5, "DECAY")
knob("SUSTAIN", 132, 42.5, "SUSTAIN")
knob("TREM", 147, 42.5, "TREMOLO")
lamps("SPLIT", 123, 12, 58.5, ["LAYER", "LOW", "HIGH"], "SPLIT")
knob("ATK", 117, 70.5, "ATTACK", "small")
knob("TRATE", 132, 70.5, "T.RATE", "small")
knob("VEL", 147, 70.5, "VEL", "small")
button("HOLD", 96, 64.5, "SUSTAIN")

# ---------------------------------------------------------------- effects, master
section(3, 80.5, W - 6, 21.8, "EFFECTS · MASTER")
for i, (n, l, k, hot) in enumerate([("BAL", "BALANCE", "knob", True), ("ANIM", "ANIMATE", "knob", False),
                                     ("ARATE", "A.RATE", "small", False), ("PHASER", "PHASER", "knob", False),
                                     ("PRATE", "P.RATE", "small", False), ("VERB", "REVERB", "knob", False),
                                     ("SIZE", "SIZE", "small", False), ("VOLUME", "VOLUME", "knob", False)]):
    knob(n, 14 + i * 19.2, 93.8, l, k, hot)

# ---------------------------------------------------------------- in / out
e(f'<rect x="3" y="104.0" width="{W - 6:.2f}" height="23.8" rx="1.2" fill="{PLATE}"/>')
X = [13.5 + i * (W - 31) / 11 for i in range(12)]
ROW2 = [("IN_SOUND", "SOUND"), ("IN_DECAY", "DECAY"), ("IN_TREM", "TREM"), ("IN_BAL", "BAL"), ("IN_ANIM", "ANIM"),
        ("IN_VERB", "VERB")]
# the GENRA strip (as on every instrument): the strings take GENRA's pad track, MOD is new
x = genra_strip(jack, e, 5, 109.0, {"V/OCT": "STR_VOCT", "GATE": "STR_GATE", "VEL": "STR_VEL", "MOD": "IN_MOD"}, ACC)
# the solo's own notes (unplugged: it plays the strings' notes)
x += 2.2
_tray(e, x, 109.0, 3, DIM)
for i, (n, l) in enumerate([("SOLO_VOCT", "V/OCT"), ("SOLO_GATE", "GATE"), ("SOLO_VEL", "VEL")]):
    jack(n, x + SLOT / 2 + i * SLOT, 109.0, l)
x += 3 * SLOT + 3.5
for i, (n, l) in enumerate([("IN_REG", "REG"), ("IN_CRESC", "CRESC"), ("IN_REL", "REL"), ("IN_TONE", "TONE"),
                            ("IN_ENS", "ENS"), ("IN_OCT", "OCT")]):
    jack(n, x + SLOT / 2 + i * 10.6, 109.0, l)
for i, (n, l) in enumerate(ROW2):
    jack(n, X[i + 1], 121.0, l)
for i, (n, l) in enumerate([("OUT_ENV", "ENV"), ("OUT_STR", "STR"), ("OUT_SOLO", "SOLO"), ("OUT_L", "L"),
                            ("OUT_R", "R")]):
    jack(n, X[i + 7], 121.0, l, True)
text(X[0], 121.7, "IN", 1.7, DIM, 800, "middle", 0.3)


e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "Schmelz.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/schmelz_panel.py - do not edit", "#pragma once", "namespace zlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    lines.append(f"static const float DISPLAY[4] = {{{DISPLAY[0]:.3f}f, {DISPLAY[1]:.3f}f, {DISPLAY[2]:.3f}f, "
                 f"{DISPLAY[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "schmelz"), exist_ok=True)
    open(os.path.join(ROOT, "src", "schmelz", "SchmelzLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/Schmelz.svg and src/schmelz/SchmelzLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components + mock display
_cache = {}


def img(name, x, y):
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
    out.insert(-1, f'<svg x="{x - sw / 2:.3f}" y="{y - sh / 2:.3f}" width="{sw:.3f}" height="{sh:.3f}" '
                   f'viewBox="{vbs}" overflow="visible">{body}</svg>')


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


for kind, n, x, y in comps:
    if kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)
for n, on in (("OCT", 0), ("ENS", 2), ("SPLIT", 0)):
    x0, y0, ddx = extra[n]
    out.insert(-1, f'<circle cx="{x0 + on * ddx}" cy="{y0}" r="1.35" fill="{ACC}"/>')
dx, dy, dw, dh = DISPLAY
ptext(dx + 4, dy + 7.6, "VIOLA", 4.4, "#ffffff", 800, "start", 0.3)
ptext(dx + 4, dy + 11.7, "> CELLO 40%", 1.8, ACC, 700, "start", 0.3)
bx, bw = dx + 45, 56
out.insert(-1, f'<line x1="{bx}" y1="{dy + 5.5}" x2="{bx + bw}" y2="{dy + 5.5}" stroke="{LINE}" stroke-width="0.5"/>')
for i, n in enumerate(["VLN", "VLA", "CEL", "BAS", "BRS", "ORG", "CHO", "SYN"]):
    x = bx + i * bw / 7
    out.insert(-1, f'<circle cx="{x:.2f}" cy="{dy + 5.5}" r="0.7" fill="#3a3c46"/>')
    ptext(x, dy + 10.9, n, 1.6, TXT if i in (1, 2) else DIM, 600, "middle", 0.2)
out.insert(-1, f'<circle cx="{bx + 1.4 * bw / 7:.2f}" cy="{dy + 5.5}" r="1.4" fill="{ACC}"/>')
ptext(dx + 109, dy + 5.1, "SOLO", 1.7, DIM, 700, "start", 0.3)
ptext(dx + 109, dy + 10.7, "E-PIANO", 3.0, "#ffffff", 800, "start", 0.3)
ptext(dx + 133, dy + 5.1, "VOICES 4", 1.6, DIM, 600, "start", 0.3)
for i in range(16):
    out.insert(-1, f'<rect x="{dx + 133 + i * 1.3:.2f}" y="{dy + 8.5}" width="0.9" height="2.2" rx="0.2" '
                   f'fill="{ACC if i < 4 else "#2a2b33"}"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1400", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
